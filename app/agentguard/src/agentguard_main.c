/* SPDX-License-Identifier: Apache-2.0 */

#include <nuttx/config.h>

#include "agentguard/core.h"
#include "agentguard/display_preview.h"
#include "agentguard/display_regions.h"
#include "agentguard/display_ui.h"
#include "agentguard/face_fallback.h"
#include "agentguard/face_presence.h"
#include "agentguard/frame_timing.h"
#include "agentguard/lcd_bounce.h"
#include "agentguard/lcd_timing.h"
#include "agentguard/lcd_transfer.h"
#include "agentguard/storage.h"
#include "agentguard/thread_priority.h"
#include "agentguard/vision.h"
#ifdef CONFIG_AGENTGUARD_ESP_DL
#  include "agentguard/espdl_tie_selftest.h"
#  include "agentguard/vision_model.h"
#endif

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include <nuttx/board.h>
#include <nuttx/cache.h>
#include <nuttx/input/buttons.h>
#include <nuttx/lcd/lcd_dev.h>
#include <nuttx/leds/userled.h>
#include <nuttx/video/video.h>

/* ESP32-S3-EYE board registration entry points.  AgentGuard initializes only
 * the hardware it needs so an unrelated board peripheral cannot block the
 * LCD status interface during the monolithic BOARDIOC_INIT sequence.
 */

extern int board_lcd_initialize(void);
extern int board_camera_initialize(void);
extern int board_i2c_init(void);

#ifndef CONFIG_AGENTGUARD_SERVER_IPV4
#  define CONFIG_AGENTGUARD_SERVER_IPV4 "192.168.1.100"
#endif
#ifndef CONFIG_AGENTGUARD_SERVER_PORT
#  define CONFIG_AGENTGUARD_SERVER_PORT 8080
#endif
#ifndef CONFIG_AGENTGUARD_SHARED_TOKEN
#  define CONFIG_AGENTGUARD_SHARED_TOKEN "change-me-before-deployment"
#endif
#ifndef CONFIG_AGENTGUARD_LOG_PATH
#  define CONFIG_AGENTGUARD_LOG_PATH "/mnt/spif/agentguard-events.jsonl"
#endif
#ifndef CONFIG_AGENTGUARD_LOG_RETENTION_DAYS
#  define CONFIG_AGENTGUARD_LOG_RETENTION_DAYS 7
#endif
#ifndef CONFIG_AGENTGUARD_LOG_MAX_BYTES
#  define CONFIG_AGENTGUARD_LOG_MAX_BYTES 524288
#endif

#define AG_CAMERA_PATH "/dev/video0"
#define AG_LCD_PATH "/dev/lcd0"
#define AG_LED_PATH "/dev/userleds"
#define AG_BUTTON_PATH "/dev/buttons"
#define AG_WIDTH 320
#define AG_HEIGHT 240
#define AG_FRAME_BYTES (AG_WIDTH * AG_HEIGHT * 2)
#define AG_LCD_WIDTH 240
#define AG_LCD_HEIGHT 240
#define AG_LCD_FRAME_PIXELS (AG_LCD_WIDTH * AG_LCD_HEIGHT)
#define AG_LCD_FRAME_BYTES (AG_LCD_FRAME_PIXELS * 2)
#define AG_PREVIEW_WIDTH 200
#define AG_PREVIEW_HEIGHT 150
#define AG_PREVIEW_X ((AG_LCD_WIDTH - AG_PREVIEW_WIDTH) / 2)
#define AG_PREVIEW_Y \
  (AG_UI_HEADER_HEIGHT + \
   (AG_LCD_HEIGHT - AG_UI_HEADER_HEIGHT - AG_UI_FOOTER_HEIGHT - \
    AG_PREVIEW_HEIGHT) / 2)
#define AG_LCD_HEADER_PIXELS (AG_LCD_WIDTH * AG_UI_HEADER_HEIGHT)
#define AG_LCD_FOOTER_PIXELS (AG_LCD_WIDTH * AG_UI_FOOTER_HEIGHT)
#define AG_LCD_HUD_PIXELS (AG_LCD_HEADER_PIXELS + AG_LCD_FOOTER_PIXELS)
/* Keep this allocation larger than the remaining internal DRAM region so
 * NuttX places the non-DMA HUD history in external RAM. */
#define AG_LCD_HUD_ALLOCATION_BYTES (64 * 1024)
#define AG_LCD_BOUNCE_PIXELS \
  (CONFIG_ESP32S3_SPI_DMA_BUFSIZE / sizeof(uint16_t))
#define AG_LCD_BOUNCE_ROWS \
  (AG_LCD_BOUNCE_PIXELS / AG_LCD_WIDTH)
#define AG_BUFFER_COUNT 3
#define AG_DISPLAY_REFRESH_US 80000
#define AG_DISPLAY_THREAD_PRIORITY 110
#define AG_CAMERA_FRAME_TIMEOUT_MS 3000
#define AG_CAMERA_RESTART_DELAY_US 250000
#define AG_CAMERA_WATCHDOG_POLL_US 50000
#define AG_FACE_PRESENCE_HOLD_MS 1500
#define AG_PC_CONNECT_TIMEOUT_MS 500
#define AG_LOG_COMPACT_INTERVAL_MS (6ull * 60ull * 60ull * 1000ull)

static uint16_t g_agentguard_lcd_bounce[AG_LCD_BOUNCE_PIXELS]
  __attribute__((aligned(64)));

_Static_assert(CONFIG_ESP32S3_SPI_DMA_BUFSIZE %
               (AG_LCD_WIDTH * sizeof(uint16_t)) == 0,
               "LCD DMA buffer must contain whole display rows");

static const struct ag_preview_area g_agentguard_preview_area =
{
  .x = AG_PREVIEW_X,
  .y = AG_PREVIEW_Y,
  .width = AG_PREVIEW_WIDTH,
  .height = AG_PREVIEW_HEIGHT
};

static const struct ag_preview_area g_agentguard_display_preview_area =
{
  .x = AG_LCD_WIDTH - AG_PREVIEW_X - AG_PREVIEW_WIDTH,
  .y = AG_LCD_HEIGHT - AG_PREVIEW_Y - AG_PREVIEW_HEIGHT,
  .width = AG_PREVIEW_WIDTH,
  .height = AG_PREVIEW_HEIGHT
};

struct ag_video
{
  int fd;
  uint16_t *buffers[AG_BUFFER_COUNT];
};

struct ag_display
{
  int fd;
  uint16_t width;
  uint16_t height;
};

struct ag_lcd_submit_context
{
  struct ag_display *display;
  struct lcddev_area_s *area;
  struct ag_lcd_submit_timing_state *submit_timing;
};

struct ag_camera_watchdog
{
  pthread_mutex_t lock;
  int fd;
  uint64_t deadline_ms;
  bool timed_out;
};

struct ag_display_worker
{
  pthread_mutex_t lock;
  struct ag_display *display;
  uint16_t *latest_pixels;
  uint16_t *draw_pixels;
  uint16_t *screen_pixels;
  uint16_t *hud_pixels;
  struct ag_display_regions_state regions;
  struct ag_ui_status status;
  struct ag_face_box face;
  uint64_t last_camera_ms;
  bool have_frame;
};

/* Updated by the ESP32-S3-EYE camera board layer while /dev/video0 is being
 * registered.  Volatile makes registration progress visible even while the
 * main thread is blocked inside a board or V4L2 call.
 */

volatile uint8_t g_agentguard_camera_phase;
volatile uint32_t g_agentguard_camera_irq_count;

static void ag_display_camera_phase(struct ag_display_worker *worker,
                                    uint8_t phase)
{
  if (worker->latest_pixels == NULL)
    {
      return;
    }

  pthread_mutex_lock(&worker->lock);
  worker->status.camera_phase = phase;
  g_agentguard_camera_phase = phase;
  pthread_mutex_unlock(&worker->lock);
}

static uint64_t ag_now_ms(void)
{
  struct timespec now;

  clock_gettime(CLOCK_MONOTONIC, &now);
  return (uint64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

static uint64_t ag_unix_ms(void)
{
  struct timespec now;

  if (clock_gettime(CLOCK_REALTIME, &now) < 0 || now.tv_sec < 0)
    {
      return 0;
    }

  return (uint64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

static void ag_default_log_policy(struct ag_log_policy *policy)
{
  policy->retention_ms =
    (uint64_t)CONFIG_AGENTGUARD_LOG_RETENTION_DAYS * 24 * 60 * 60 * 1000;
  policy->max_bytes = CONFIG_AGENTGUARD_LOG_MAX_BYTES;
}

static void ag_video_close(struct ag_video *video)
{
  enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  int i;

  if (video->fd >= 0)
    {
      ioctl(video->fd, VIDIOC_STREAMOFF, (uintptr_t)&type);
      close(video->fd);
      video->fd = -1;
    }

  for (i = 0; i < AG_BUFFER_COUNT; i++)
    {
      free(video->buffers[i]);
      video->buffers[i] = NULL;
    }
}

static int ag_video_open(struct ag_video *video,
                         struct ag_display_worker *display_worker)
{
  struct v4l2_format format;
  struct v4l2_requestbuffers request;
  struct v4l2_buffer buffer;
  enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  int i;

  memset(video, 0, sizeof(*video));
  video->fd = -1;
  ag_display_camera_phase(display_worker, 2);
  video->fd = open(AG_CAMERA_PATH, O_RDONLY);
  if (video->fd < 0)
    {
      fprintf(stderr, "agentguard: cannot open %s: %d\n",
              AG_CAMERA_PATH, errno);
      return -1;
    }

  memset(&format, 0, sizeof(format));
  format.type = type;
  format.fmt.pix.width = AG_WIDTH;
  format.fmt.pix.height = AG_HEIGHT;
  format.fmt.pix.field = V4L2_FIELD_ANY;
  format.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB565;
  ag_display_camera_phase(display_worker, 3);
  if (ioctl(video->fd, VIDIOC_S_FMT, (uintptr_t)&format) < 0)
    {
      fprintf(stderr, "agentguard: VIDIOC_S_FMT failed: %d\n", errno);
      goto error;
    }

  memset(&request, 0, sizeof(request));
  request.type = type;
  request.memory = V4L2_MEMORY_USERPTR;
  request.count = AG_BUFFER_COUNT;
  request.mode = V4L2_BUF_MODE_RING;
  ag_display_camera_phase(display_worker, 4);
  if (ioctl(video->fd, VIDIOC_REQBUFS, (uintptr_t)&request) < 0)
    {
      fprintf(stderr, "agentguard: VIDIOC_REQBUFS failed: %d\n", errno);
      goto error;
    }

  for (i = 0; i < AG_BUFFER_COUNT; i++)
    {
      video->buffers[i] = memalign(32, AG_FRAME_BYTES);
      if (video->buffers[i] == NULL)
        {
          fprintf(stderr, "agentguard: no PSRAM for camera buffer\n");
          goto error;
        }

      memset(&buffer, 0, sizeof(buffer));
      buffer.type = type;
      buffer.memory = V4L2_MEMORY_USERPTR;
      buffer.index = i;
      buffer.m.userptr = (uintptr_t)video->buffers[i];
      buffer.length = AG_FRAME_BYTES;
      if (ioctl(video->fd, VIDIOC_QBUF, (uintptr_t)&buffer) < 0)
        {
          fprintf(stderr, "agentguard: VIDIOC_QBUF failed: %d\n", errno);
          goto error;
        }
    }

  ag_display_camera_phase(display_worker, 5);
  if (ioctl(video->fd, VIDIOC_STREAMON, (uintptr_t)&type) < 0)
    {
      fprintf(stderr, "agentguard: VIDIOC_STREAMON failed: %d\n", errno);
      goto error;
    }

  return 0;

error:
  ag_video_close(video);
  return -1;
}

static void *ag_camera_watchdog_main(void *argument)
{
  struct ag_camera_watchdog *watchdog = argument;

  for (;;)
    {
      int fd = -1;

      usleep(AG_CAMERA_WATCHDOG_POLL_US);
      pthread_mutex_lock(&watchdog->lock);
      if (watchdog->deadline_ms != 0 &&
          ag_now_ms() >= watchdog->deadline_ms)
        {
          watchdog->timed_out = true;
          fd = watchdog->fd;
        }

      pthread_mutex_unlock(&watchdog->lock);

      if (fd >= 0)
        {
          g_agentguard_camera_phase =
            g_agentguard_camera_irq_count == 0 ? 17 : 18;
          ioctl(fd, VIDIOC_CANCEL_DQBUF,
                (uintptr_t)V4L2_BUF_TYPE_VIDEO_CAPTURE);
        }
    }

  return NULL;
}

static int ag_next_frame(struct ag_video *video,
                         struct ag_camera_watchdog *watchdog,
                         struct v4l2_buffer *buffer)
{
  bool timed_out;
  int result;

  pthread_mutex_lock(&watchdog->lock);
  watchdog->fd = video->fd;
  watchdog->timed_out = false;
  watchdog->deadline_ms = ag_now_ms() + AG_CAMERA_FRAME_TIMEOUT_MS;
  pthread_mutex_unlock(&watchdog->lock);

  memset(buffer, 0, sizeof(*buffer));
  buffer->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  buffer->memory = V4L2_MEMORY_USERPTR;
  result = ioctl(video->fd, VIDIOC_DQBUF, (uintptr_t)buffer);

  pthread_mutex_lock(&watchdog->lock);
  watchdog->deadline_ms = 0;
  timed_out = watchdog->timed_out;
  watchdog->timed_out = false;
  pthread_mutex_unlock(&watchdog->lock);

  if (result < 0 && timed_out)
    {
      errno = ETIMEDOUT;
    }

  return result;
}

static int ag_video_restart(struct ag_video *video,
                            struct ag_display_worker *display_worker,
                            struct ag_frame_timing_state *frame_timing)
{
#ifdef CONFIG_AGENTGUARD_ESP_DL
  ag_vision_model_reset_face_diagnostics();
#endif
  ag_frame_timing_reset(frame_timing);
  ag_video_close(video);
  usleep(AG_CAMERA_RESTART_DELAY_US);
  return ag_video_open(video, display_worker);
}

static void ag_display_close(struct ag_display *display)
{
  if (display->fd >= 0)
    {
      close(display->fd);
      display->fd = -1;
    }
}

static int ag_display_open(struct ag_display *display)
{
  struct fb_videoinfo_s video_info;
  struct lcd_planeinfo_s plane_info;

  memset(display, 0, sizeof(*display));
  display->fd = -1;
  display->fd = open(AG_LCD_PATH, O_WRONLY);
  if (display->fd < 0)
    {
      fprintf(stderr, "agentguard: LCD preview unavailable (%s): %d\n",
              AG_LCD_PATH, errno);
      return -1;
    }

  memset(&video_info, 0, sizeof(video_info));
  memset(&plane_info, 0, sizeof(plane_info));
  if (ioctl(display->fd, LCDDEVIO_GETVIDEOINFO,
            (uintptr_t)&video_info) < 0 ||
      ioctl(display->fd, LCDDEVIO_GETPLANEINFO,
            (uintptr_t)&plane_info) < 0)
    {
      fprintf(stderr, "agentguard: cannot query LCD: %d\n", errno);
      goto error;
    }

  if (video_info.fmt != FB_FMT_RGB16_565 || plane_info.bpp != 16 ||
      video_info.xres != AG_LCD_WIDTH || video_info.yres != AG_LCD_HEIGHT)
    {
      fprintf(stderr, "agentguard: unsupported LCD format=%u bpp=%u\n",
              video_info.fmt, plane_info.bpp);
      goto error;
    }

  if (ioctl(display->fd, LCDDEVIO_SETPOWER,
            (uintptr_t)CONFIG_LCD_MAXPOWER) < 0)
    {
      fprintf(stderr, "agentguard: cannot power LCD: %d\n", errno);
      goto error;
    }

  display->width = video_info.xres;
  display->height = video_info.yres;
  return 0;

error:
  ag_display_close(display);
  return -1;
}

static void ag_draw_face_box(uint16_t *pixels, uint16_t width,
                             uint16_t height,
                             const struct ag_face_box *face, uint16_t color)
{
  unsigned int left;
  unsigned int right;
  unsigned int top;
  unsigned int bottom;
  unsigned int x;
  unsigned int y;
  unsigned int thickness;

  if (face->width == 0 || face->height == 0 || face->x >= width ||
      face->y >= height)
    {
      return;
    }

  left = face->x;
  top = face->y;
  right = left + face->width - 1;
  bottom = top + face->height - 1;
  if (right >= width) right = width - 1;
  if (bottom >= height) bottom = height - 1;

  for (thickness = 0; thickness < 2; thickness++)
    {
      unsigned int box_left = left + thickness;
      unsigned int box_top = top + thickness;
      unsigned int box_right;
      unsigned int box_bottom;

      if (right < thickness || bottom < thickness)
        {
          break;
        }

      box_right = right - thickness;
      box_bottom = bottom - thickness;
      if (box_left > box_right || box_top > box_bottom)
        {
          break;
        }

      for (x = box_left; x <= box_right; x++)
        {
          pixels[box_top * width + x] = color;
          pixels[box_bottom * width + x] = color;
        }

      for (y = box_top; y <= box_bottom; y++)
        {
          pixels[y * width + box_left] = color;
          pixels[y * width + box_right] = color;
        }
    }
}

static bool ag_lcd_read_ms(void *context, uint64_t *value)
{
  struct timespec now;

  (void)context;
  if (value == NULL || clock_gettime(CLOCK_MONOTONIC, &now) < 0 ||
      now.tv_sec < 0)
    {
      return false;
    }

  *value = (uint64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
  return true;
}

static void ag_lcd_clean(void *context, uintptr_t start, uintptr_t end)
{
  (void)context;
  up_clean_dcache(start, end);
}

static int ag_lcd_submit(void *argument)
{
  struct ag_lcd_submit_context *context = argument;

  return ioctl(context->display->fd, LCDDEVIO_PUTAREA,
               (uintptr_t)context->area);
}

static int ag_lcd_submit_chunk(void *argument, const uint16_t *pixels,
                               uint16_t first_column, uint16_t first_row,
                               uint16_t row_count,
                               uint16_t width)
{
  struct ag_lcd_submit_context *context = argument;
  struct ag_lcd_transfer_ops transfer_ops;
  struct ag_lcd_timing_state chunk_timing;
  struct lcddev_area_s *area = context->area;
  int result;

  area->row_start = first_row;
  area->row_end = first_row + row_count - 1;
  area->col_start = first_column;
  area->col_end = first_column + width - 1;
  area->stride = width * sizeof(*pixels);
  area->data = (uint8_t *)pixels;

  transfer_ops.read_ms = ag_lcd_read_ms;
  transfer_ops.clean = ag_lcd_clean;
  transfer_ops.submit = ag_lcd_submit;
  transfer_ops.context = context;
  ag_lcd_timing_reset(&chunk_timing);
  result = ag_lcd_transfer_run(&transfer_ops, (uintptr_t)pixels,
                               (size_t)row_count * width * sizeof(*pixels),
                               &chunk_timing);
  ag_lcd_submit_timing_record(context->submit_timing, &chunk_timing);
  return result;
}

static int ag_display_submit_area(void *argument, const uint16_t *pixels,
                                  uint16_t stride,
                                  uint16_t source_height,
                                  uint16_t x, uint16_t y,
                                  uint16_t width, uint16_t height)
{
  struct ag_lcd_submit_context *context = argument;
  struct ag_lcd_bounce_ops bounce_ops;
  struct ag_lcd_timing_state area_timing;

  bounce_ops.read_ms = ag_lcd_read_ms;
  bounce_ops.submit = ag_lcd_submit_chunk;
  bounce_ops.context = context;
  ag_lcd_timing_reset(&area_timing);
  return ag_lcd_bounce_area(&bounce_ops, pixels, stride, source_height,
                            x, y, x, y, width, height,
                            g_agentguard_lcd_bounce,
                            AG_LCD_BOUNCE_ROWS, &area_timing);
}

static void ag_display_frame(struct ag_display_worker *worker,
                             struct ag_lcd_timing_state *lcd_timing,
                             struct ag_lcd_submit_timing_state *submit_timing)
{
  struct lcddev_area_s area;
  struct ag_lcd_submit_context context;
  struct ag_display_regions_ops regions_ops;
  uint64_t started_ms = 0;
  uint64_t finished_ms = 0;
  bool have_started;
  bool have_finished;
  int result;

  if (worker->display->fd < 0)
    {
      return;
    }

  memset(&area, 0, sizeof(area));
  context.display = worker->display;
  context.area = &area;
  context.submit_timing = submit_timing;
  regions_ops.submit = ag_display_submit_area;
  regions_ops.context = &context;
  ag_lcd_submit_timing_reset(submit_timing);
  ag_lcd_timing_invalidate(lcd_timing);
  have_started = ag_lcd_read_ms(NULL, &started_ms);
  result = ag_display_regions_update(&regions_ops,
                                     worker->screen_pixels,
                                     AG_LCD_WIDTH, AG_LCD_HEIGHT,
                                     &g_agentguard_display_preview_area,
                                     AG_UI_FOOTER_HEIGHT,
                                     AG_UI_HEADER_HEIGHT,
                                     &worker->regions);
  have_finished = ag_lcd_read_ms(NULL, &finished_ms);
  if (result == 0 && have_started && have_finished)
    {
      ag_lcd_timing_update(lcd_timing, started_ms, finished_ms);
    }

  if (result < 0)
    {
      fprintf(stderr, "agentguard: LCD preview stopped: %d\n", errno);
      ag_display_close(worker->display);
    }
}

static void *ag_display_worker_main(void *argument)
{
  struct ag_display_worker *worker = argument;
  struct ag_ui_status status;
  struct ag_face_box face;
  struct ag_face_box mapped_face;
  struct ag_lcd_timing_state lcd_timing;
  struct ag_lcd_submit_timing_state submit_timing;
  uint64_t last_camera_ms;
  uint64_t now_ms;
  bool have_frame;

  ag_lcd_timing_reset(&lcd_timing);
  ag_lcd_submit_timing_reset(&submit_timing);

  for (;;)
    {
      pthread_mutex_lock(&worker->lock);
      have_frame = worker->have_frame;
      if (have_frame)
        {
          memcpy(worker->draw_pixels, worker->latest_pixels,
                 AG_FRAME_BYTES);
        }

      status = worker->status;
      if (!have_frame)
        {
          status.camera_phase = g_agentguard_camera_phase;
        }
#ifdef CONFIG_AGENTGUARD_ESP_DL
      status.tie_selftest_stage =
        ag_espdl_tie_conv_selftest_get_stage();
      status.tie_selftest_valid =
        ag_espdl_tie_conv_selftest_get_result(&status.tie_ram_pass,
                                              &status.tie_flash_pass);
#endif
      face = worker->face;
      last_camera_ms = worker->last_camera_ms;
      pthread_mutex_unlock(&worker->lock);

      status.lcd_write_valid = lcd_timing.valid;
      status.lcd_write_ms = lcd_timing.write_ms;
      status.lcd_submit_timing_valid = submit_timing.valid;
      status.lcd_submit_sum_ms = submit_timing.sum_ms;
      status.lcd_submit_max_ms = submit_timing.max_ms;

      if (!have_frame)
        {
          memset(&face, 0, sizeof(face));
        }

      memset(worker->screen_pixels, 0, AG_LCD_FRAME_BYTES);
      if (have_frame &&
          !ag_preview_scale_rgb565(worker->screen_pixels,
                                   AG_LCD_WIDTH, AG_LCD_HEIGHT,
                                   worker->draw_pixels,
                                   AG_WIDTH, AG_HEIGHT,
                                   &g_agentguard_preview_area))
        {
          have_frame = false;
          memset(&face, 0, sizeof(face));
        }

      if (have_frame &&
          !ag_preview_map_face(&face, AG_WIDTH, AG_HEIGHT,
                               &g_agentguard_preview_area, &mapped_face))
        {
          memset(&face, 0, sizeof(face));
        }
      else if (have_frame)
        {
          face = mapped_face;
        }

      now_ms = ag_now_ms();
      status.activity_on = ((now_ms / 500u) & 1u) != 0;
      status.camera_stale = !have_frame ||
                            now_ms - last_camera_ms >=
                            AG_CAMERA_FRAME_TIMEOUT_MS;
      ag_draw_face_box(worker->screen_pixels, AG_LCD_WIDTH, AG_LCD_HEIGHT,
                       &face, ag_ui_face_color(&status));
      ag_ui_render_oriented_rgb565(worker->screen_pixels,
                                   AG_LCD_WIDTH, AG_LCD_HEIGHT,
                                   worker->display->width,
                                   worker->display->height, &status);
      ag_display_frame(worker, &lcd_timing, &submit_timing);

      usleep(AG_DISPLAY_REFRESH_US);
    }

  return NULL;
}

static int ag_display_worker_start(struct ag_display_worker *worker,
                                   struct ag_display *display)
{
  pthread_attr_t attr;
  pthread_t thread;
  int result;

  memset(worker, 0, sizeof(*worker));
  worker->display = display;
  if (display->fd < 0)
    {
      return OK;
    }

  worker->latest_pixels = memalign(32, AG_FRAME_BYTES);
  worker->draw_pixels = memalign(32, AG_FRAME_BYTES);
  worker->screen_pixels = memalign(32, AG_LCD_FRAME_BYTES);
  worker->hud_pixels = memalign(32, AG_LCD_HUD_ALLOCATION_BYTES);
  if (worker->latest_pixels == NULL || worker->draw_pixels == NULL ||
      worker->screen_pixels == NULL || worker->hud_pixels == NULL)
    {
      free(worker->latest_pixels);
      free(worker->draw_pixels);
      free(worker->screen_pixels);
      free(worker->hud_pixels);
      worker->latest_pixels = NULL;
      worker->draw_pixels = NULL;
      worker->screen_pixels = NULL;
      worker->hud_pixels = NULL;
      return -1;
    }

  memset(worker->latest_pixels, 0, AG_FRAME_BYTES);
  memset(worker->draw_pixels, 0, AG_FRAME_BYTES);
  memset(worker->screen_pixels, 0, AG_LCD_FRAME_BYTES);
  memset(worker->hud_pixels, 0,
         AG_LCD_HUD_PIXELS * sizeof(uint16_t));
  worker->regions.header_snapshot = worker->hud_pixels;
  worker->regions.footer_snapshot =
    worker->hud_pixels + AG_LCD_FOOTER_PIXELS;
  if (ag_thread_attr_init_priority(&attr, AG_DISPLAY_THREAD_PRIORITY) != 0)
    {
      goto fail_buffers;
    }

  if (pthread_mutex_init(&worker->lock, NULL) != 0)
    {
      pthread_attr_destroy(&attr);
      goto fail_buffers;
    }

  result = pthread_create(&thread, &attr, ag_display_worker_main, worker);
  pthread_attr_destroy(&attr);
  if (result != 0)
    {
      pthread_mutex_destroy(&worker->lock);
      goto fail_buffers;
    }

  pthread_detach(thread);
  return OK;

fail_buffers:
  free(worker->latest_pixels);
  free(worker->draw_pixels);
  free(worker->screen_pixels);
  free(worker->hud_pixels);
  worker->latest_pixels = NULL;
  worker->draw_pixels = NULL;
  worker->screen_pixels = NULL;
  worker->hud_pixels = NULL;
  return -1;
}

static void ag_display_publish(struct ag_display_worker *worker,
                               const uint16_t *pixels,
                               const struct ag_ui_status *status,
                               const struct ag_face_box *face,
                               uint64_t now_ms)
{
  if (worker->latest_pixels == NULL)
    {
      return;
    }

  pthread_mutex_lock(&worker->lock);
  memcpy(worker->latest_pixels, pixels, AG_FRAME_BYTES);
  worker->status = *status;
  worker->face = *face;
  worker->last_camera_ms = now_ms;
  worker->have_frame = true;
  pthread_mutex_unlock(&worker->lock);
}

static void ag_log_event(const char *json, uint64_t monotonic_ms)
{
  struct ag_log_policy policy;
  static uint64_t last_compact_ms;

  ag_default_log_policy(&policy);
  if (last_compact_ms == 0 ||
      monotonic_ms - last_compact_ms >= AG_LOG_COMPACT_INTERVAL_MS)
    {
      if (ag_log_compact(CONFIG_AGENTGUARD_LOG_PATH, ag_unix_ms(),
                         &policy) < 0)
        {
          return;
        }

      last_compact_ms = monotonic_ms;
    }

  if (ag_log_append(CONFIG_AGENTGUARD_LOG_PATH, json, ag_unix_ms(),
                    &policy) < 0)
    {
      return;
    }
}

static int ag_print_stats(void)
{
  struct ag_log_stats stats;

  if (ag_log_read_stats(CONFIG_AGENTGUARD_LOG_PATH, &stats) < 0)
    {
      fprintf(stderr, "agentguard: cannot read behavior log %s: %d\n",
              CONFIG_AGENTGUARD_LOG_PATH, errno);
      return EXIT_FAILURE;
    }

  printf("AgentGuard stats: total=%lu breaks=%lu sedentary=%lu posture=%lu "
         "locks=%lu acknowledgements=%lu malformed=%lu\n",
         (unsigned long)stats.total_events,
         (unsigned long)ag_log_event_count(&stats, AG_EVENT_BREAK_STARTED),
         (unsigned long)ag_log_event_count(&stats, AG_EVENT_SEDENTARY_ALERT),
         (unsigned long)ag_log_event_count(&stats, AG_EVENT_POSTURE_ALERT),
         (unsigned long)ag_log_event_count(&stats, AG_EVENT_LOCK_SCREEN),
         (unsigned long)ag_log_event_count(&stats, AG_EVENT_ACKNOWLEDGED),
         (unsigned long)stats.malformed_lines);
  if (stats.first_unix_ms == 0)
    {
      printf("AgentGuard stats: wall clock unavailable in retained records\n");
    }
  else
    {
      printf("AgentGuard stats: unix_ms=%llu..%llu\n",
             (unsigned long long)stats.first_unix_ms,
             (unsigned long long)stats.last_unix_ms);
    }

  return EXIT_SUCCESS;
}

static int ag_prune_log(void)
{
  struct ag_log_policy policy;

  ag_default_log_policy(&policy);
  if (ag_log_compact(CONFIG_AGENTGUARD_LOG_PATH, ag_unix_ms(), &policy) < 0)
    {
      fprintf(stderr, "agentguard: cannot compact behavior log %s: %d\n",
              CONFIG_AGENTGUARD_LOG_PATH, errno);
      return EXIT_FAILURE;
    }

  printf("AgentGuard behavior log compacted\n");
  return EXIT_SUCCESS;
}

static int ag_post_event(const char *json)
{
  struct sockaddr_in address;
  struct pollfd pollfd;
  struct timeval send_timeout;
  char request[768];
  socklen_t error_length;
  int connect_error;
  int flags;
  int length;
  int result;
  int sock;

  sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0)
    {
      return -1;
    }

  memset(&address, 0, sizeof(address));
  address.sin_family = AF_INET;
  address.sin_port = htons(CONFIG_AGENTGUARD_SERVER_PORT);
  if (inet_pton(AF_INET, CONFIG_AGENTGUARD_SERVER_IPV4,
                &address.sin_addr) != 1)
    {
      close(sock);
      return -1;
    }

  flags = fcntl(sock, F_GETFL, 0);
  if (flags < 0 || fcntl(sock, F_SETFL, flags | O_NONBLOCK) < 0)
    {
      close(sock);
      return -1;
    }

  result = connect(sock, (struct sockaddr *)&address, sizeof(address));
  if (result < 0 && errno == EINPROGRESS)
    {
      memset(&pollfd, 0, sizeof(pollfd));
      pollfd.fd = sock;
      pollfd.events = POLLOUT;
      do
        {
          result = poll(&pollfd, 1, AG_PC_CONNECT_TIMEOUT_MS);
        }
      while (result < 0 && errno == EINTR);

      if (result > 0 &&
          (pollfd.revents & (POLLOUT | POLLERR | POLLHUP)) != 0)
        {
          connect_error = 0;
          error_length = sizeof(connect_error);
          if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &connect_error,
                         &error_length) < 0 || connect_error != 0)
            {
              result = -1;
            }
          else
            {
              result = 0;
            }
        }
      else
        {
          result = -1;
        }
    }

  if (result < 0 || fcntl(sock, F_SETFL, flags) < 0)
    {
      close(sock);
      return -1;
    }

  send_timeout.tv_sec = 0;
  send_timeout.tv_usec = AG_PC_CONNECT_TIMEOUT_MS * 1000;
  if (setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &send_timeout,
                 sizeof(send_timeout)) < 0)
    {
      close(sock);
      return -1;
    }

  length = snprintf(request, sizeof(request),
                    "POST /event HTTP/1.1\r\n"
                    "Host: %s:%d\r\n"
                    "Content-Type: application/json\r\n"
                    "Authorization: Bearer %s\r\n"
                    "Content-Length: %u\r\nConnection: close\r\n\r\n%s",
                    CONFIG_AGENTGUARD_SERVER_IPV4,
                    CONFIG_AGENTGUARD_SERVER_PORT,
                    CONFIG_AGENTGUARD_SHARED_TOKEN,
                    (unsigned int)strlen(json), json);
  if (length <= 0 || length >= (int)sizeof(request))
    {
      close(sock);
      return -1;
    }

  length = send(sock, request, length, 0);
  close(sock);
  return length < 0 ? -1 : 0;
}

static void ag_set_led(int led_fd, bool enabled)
{
  if (led_fd >= 0)
    {
      ioctl(led_fd, ULEDIOC_SETALL, enabled ? 1 : 0);
    }
}

static enum ag_command ag_read_button(int button_fd, bool *was_pressed)
{
  btn_buttonset_t sample = 0;
  bool pressed;

  if (button_fd < 0 || read(button_fd, &sample, sizeof(sample)) <= 0)
    {
      return AG_COMMAND_NONE;
    }

  pressed = sample != 0;
  if (pressed && !*was_pressed)
    {
      *was_pressed = true;
      return AG_COMMAND_ACKNOWLEDGE;
    }

  if (!pressed)
    {
      *was_pressed = false;
    }

  return AG_COMMAND_NONE;
}

static void ag_dispatch_events(uint32_t events, uint64_t now,
                               const struct ag_state *state, int led_fd)
{
  unsigned int bit;
  char json[256];

  for (bit = 0; bit < 31; bit++)
    {
      enum ag_event event = (enum ag_event)(1u << bit);

      if ((events & event) == 0)
        {
          continue;
        }

      ag_event_json(json, sizeof(json), event, now, state);
      ag_log_event(json, now);

      if (event == AG_EVENT_SEDENTARY_ALERT ||
          event == AG_EVENT_POSTURE_ALERT ||
          event == AG_EVENT_BLUR_SCREEN ||
          event == AG_EVENT_LOCK_SCREEN)
        {
          ag_set_led(led_fd, true);
        }
      else if (event == AG_EVENT_ACKNOWLEDGED ||
               event == AG_EVENT_UNBLUR_SCREEN)
        {
          ag_set_led(led_fd, false);
        }

      if (event == AG_EVENT_SEDENTARY_ALERT ||
          event == AG_EVENT_POSTURE_ALERT ||
          event == AG_EVENT_BLUR_SCREEN ||
          event == AG_EVENT_UNBLUR_SCREEN ||
          event == AG_EVENT_LOCK_SCREEN)
        {
          ag_post_event(json);
        }
    }
}

static int ag_run(void)
{
  struct ag_video video;
  struct ag_display display;
  struct ag_vision_context vision;
  struct ag_vision_result vision_result;
  struct ag_face_presence_state face_presence;
  struct ag_config config;
  struct ag_state state;
  struct ag_observation observation;
  struct ag_ui_status ui_status;
#ifdef CONFIG_AGENTGUARD_ESP_DL
  struct ag_vision_model_diagnostics model_diagnostics;
  struct ag_face_diag_snapshot face_diagnostics;
#endif
  struct ag_camera_watchdog watchdog;
  struct ag_display_worker display_worker;
  struct ag_frame_timing_state frame_timing;
  struct v4l2_buffer frame;
  pthread_t watchdog_thread;
  uint64_t dequeue_started_ms;
  uint64_t dequeue_finished_ms;
  bool was_pressed = false;
  uint32_t frame_sequence = 0;
  int led_fd;
  int button_fd;

  while (ag_display_open(&display) < 0)
    {
      fprintf(stderr, "agentguard: LCD initialization failed; retrying\n");
      usleep(AG_CAMERA_RESTART_DELAY_US);
    }

  while (ag_display_worker_start(&display_worker, &display) < 0)
    {
      fprintf(stderr, "agentguard: LCD worker allocation failed; retrying\n");
      usleep(AG_CAMERA_RESTART_DELAY_US);
    }

  /* Let the diagnostic UI replace the controller's default white screen
   * before camera setup begins.  Camera ioctls may block on faulty hardware,
   * but the independent LCD worker must remain useful in that case.
   */

  usleep(AG_DISPLAY_REFRESH_US * 2u);

#ifdef CONFIG_AGENTGUARD_ESP_DL
  ag_espdl_tie_conv_selftest_run_once();
#endif

  /* The OV2640 sensor setup performs its transfers through I2C0.  The
   * normal board bring-up initializes and pins that controller before the
   * camera is registered.  Keep the same ordering in the minimal AgentGuard
   * bring-up; initializing it lazily from inside VIDIOC_S_FMT can leave the
   * capture open waiting forever for the sensor transaction to complete.
   */

  if (access("/dev/i2c0", F_OK) < 0)
    {
      int result = board_i2c_init();

      if (result < 0)
        {
          fprintf(stderr, "agentguard: I2C initialization failed: %d\n",
                  result);
        }
    }

  if (access(AG_CAMERA_PATH, F_OK) < 0)
    {
      ag_display_camera_phase(&display_worker, 1);
      int result = board_camera_initialize();

      if (result < 0)
        {
          fprintf(stderr, "agentguard: camera registration failed: %d\n",
                  result);
        }
    }

  memset(&video, 0, sizeof(video));
  video.fd = -1;
  while (ag_video_open(&video, &display_worker) < 0)
    {
      fprintf(stderr, "agentguard: camera initialization failed; retrying\n");
      usleep(AG_CAMERA_RESTART_DELAY_US);
    }

  memset(&watchdog, 0, sizeof(watchdog));
  watchdog.fd = -1;
  if (pthread_mutex_init(&watchdog.lock, NULL) != 0 ||
      pthread_create(&watchdog_thread, NULL, ag_camera_watchdog_main,
                     &watchdog) != 0)
    {
      fprintf(stderr, "agentguard: cannot start camera watchdog\n");
      ag_video_close(&video);
      return EXIT_FAILURE;
    }

  pthread_detach(watchdog_thread);
  ag_display_camera_phase(&display_worker, 6);
  g_agentguard_camera_irq_count = 0;

  led_fd = open(AG_LED_PATH, O_WRONLY);
  button_fd = open(AG_BUTTON_PATH, O_RDONLY | O_NONBLOCK);
  ag_vision_init(&vision);
  ag_face_presence_reset(&face_presence);
  ag_default_config(&config);
  ag_init(&state);
  ag_frame_timing_reset(&frame_timing);

  for (;;)
    {
      dequeue_started_ms = ag_now_ms();
      if (ag_next_frame(&video, &watchdog, &frame) < 0)
        {
          fprintf(stderr,
                  "agentguard: camera frame timeout/error %d; restarting\n",
                  errno);
          if (ag_video_restart(&video, &display_worker,
                               &frame_timing) < 0)
            {
              fprintf(stderr, "agentguard: camera restart failed; retrying\n");
              usleep(AG_CAMERA_RESTART_DELAY_US);
            }
          else
            {
              printf("agentguard: camera stream recovered\n");
            }

          continue;
        }

      dequeue_finished_ms = ag_now_ms();
      ag_frame_timing_update(&frame_timing,
                             dequeue_started_ms,
                             dequeue_finished_ms,
                             frame.timestamp.tv_sec,
                             frame.timestamp.tv_usec);

      if (ag_vision_process_rgb565(&vision,
                                   (uint16_t *)frame.m.userptr,
                                   AG_WIDTH, AG_HEIGHT,
                                   &vision_result) == 0)
        {
          ag_face_fallback_apply_rgb565((uint16_t *)frame.m.userptr,
                                        AG_WIDTH, AG_HEIGHT,
                                        &vision_result);
          ag_face_presence_filter(&face_presence, ag_now_ms(),
                                  AG_FACE_PRESENCE_HOLD_MS,
                                  &vision_result);
          struct ag_face_box display_face = vision_result.primary_face;

          observation.monotonic_ms = ag_now_ms();
          observation.face_count = vision_result.face_count;
          observation.posture_score = vision_result.posture_score;
          observation.command = ag_read_button(button_fd, &was_pressed);
          ag_dispatch_events(ag_step(&state, &config, &observation),
                             observation.monotonic_ms, &state, led_fd);
          memset(&ui_status, 0, sizeof(ui_status));
          ui_status.frame_sequence = ++frame_sequence;
          ui_status.camera_phase = 7;
          ui_status.face_count = vision_result.face_count;
          ui_status.posture_score = vision_result.posture_score;
          ui_status.frame_timing_valid = frame_timing.valid;
          ui_status.capture_interval_ms = frame_timing.capture_interval_ms;
          ui_status.dequeue_wait_ms = frame_timing.dequeue_wait_ms;
          ui_status.loop_interval_ms = frame_timing.loop_interval_ms;
#ifdef CONFIG_AGENTGUARD_ESP_DL
          ag_vision_model_get_diagnostics(&model_diagnostics);
          ui_status.model_diagnostics_valid = model_diagnostics.valid != 0;
          ui_status.msr_candidates = model_diagnostics.msr_candidates;
          ui_status.model_input_min = model_diagnostics.input_min;
          ui_status.model_input_max = model_diagnostics.input_max;
          ui_status.msr_score_percent = model_diagnostics.score_percent;
          memcpy(ui_status.pixel_mode_scores,
                 model_diagnostics.pixel_mode_scores,
                 sizeof(ui_status.pixel_mode_scores));
          ui_status.reference_score_percent =
            model_diagnostics.reference_score_percent;
          ui_status.reference_face_count =
            model_diagnostics.reference_face_count;
          ui_status.tie_selftest_valid =
            model_diagnostics.tie_selftest_valid != 0;
          ui_status.tie_ram_pass = model_diagnostics.tie_ram_pass != 0;
          ui_status.tie_flash_pass = model_diagnostics.tie_flash_pass != 0;
          ui_status.inference_ms = model_diagnostics.inference_ms;
          if (ag_vision_model_get_face_diagnostics(&face_diagnostics))
            {
              ui_status.face_diagnostics_valid = true;
              ui_status.face_diagnostics_generation =
                face_diagnostics.inference_sequence;
              ui_status.face_diagnostics_raw_change_valid =
                face_diagnostics.raw_change_valid;
              ui_status.face_diagnostics_raw_changed =
                face_diagnostics.raw_changed;
              ui_status.face_diagnostics_input_change_valid =
                face_diagnostics.input_change_valid;
              ui_status.face_diagnostics_input_changed =
                face_diagnostics.input_changed;
              ui_status.face_diagnostics_mnp_attempts =
                face_diagnostics.live.mnp_attempts;
              ui_status.face_diagnostics_mnp_accepted =
                face_diagnostics.live.mnp_accepted;
              ui_status.face_diagnostics_faces =
                face_diagnostics.live.final_faces;
            }
#endif
          ui_status.calibrated = vision.stable_frames >= 20;
          ui_status.reminders_paused = state.reminders_paused;
          ui_status.privacy_enabled = state.privacy_enabled;
          ui_status.sedentary_alerted = state.sedentary_alerted;
          ui_status.posture_alerted = state.posture_alerted;
          ui_status.awaiting_ack = state.awaiting_ack;
          if (state.present &&
              observation.monotonic_ms >= state.presence_since_ms)
            {
              ui_status.seated_ms =
                observation.monotonic_ms - state.presence_since_ms;
            }

          ag_ui_rotate_180_rgb565((uint16_t *)frame.m.userptr,
                                  AG_WIDTH, AG_HEIGHT);
          if (display_face.x + display_face.width <= AG_WIDTH &&
              display_face.y + display_face.height <= AG_HEIGHT)
            {
              display_face.x = AG_WIDTH - display_face.x -
                               display_face.width;
              display_face.y = AG_HEIGHT - display_face.y -
                               display_face.height;
            }

          ag_display_publish(&display_worker,
                             (uint16_t *)frame.m.userptr,
                             &ui_status, &display_face,
                             observation.monotonic_ms);
        }

      if (ioctl(video.fd, VIDIOC_QBUF, (uintptr_t)&frame) < 0)
        {
          fprintf(stderr, "agentguard: VIDIOC_QBUF failed: %d; restarting\n",
                  errno);
          ag_video_restart(&video, &display_worker, &frame_timing);
        }
    }

  ag_set_led(led_fd, false);
  if (button_fd >= 0) close(button_fd);
  if (led_fd >= 0) close(led_fd);
  ag_display_close(&display);
  ag_video_close(&video);
  return EXIT_FAILURE;
}

int main(int argc, char *argv[])
{
  if (access(AG_LCD_PATH, F_OK) < 0)
    {
      int result = board_lcd_initialize();

      if (result < 0)
        {
          fprintf(stderr, "agentguard: LCD hardware initialization failed: "
                  "%d\n", result);
          return EXIT_FAILURE;
        }

      result = lcddev_register(0);
      if (result < 0)
        {
          fprintf(stderr, "agentguard: LCD device registration failed: %d\n",
                  result);
          return EXIT_FAILURE;
        }
    }

  if (argc == 2 && strcmp(argv[1], "stats") == 0)
    {
      return ag_print_stats();
    }

  if (argc == 2 && strcmp(argv[1], "prune") == 0)
    {
      return ag_prune_log();
    }

  if (argc == 2 && strcmp(argv[1], "face-diag") == 0)
    {
#ifdef CONFIG_AGENTGUARD_ESP_DL
      struct ag_face_diag_snapshot diagnostics;
      char output[768];

      if (!ag_vision_model_get_face_diagnostics(&diagnostics) ||
          !ag_face_diag_format(output, sizeof(output), &diagnostics))
        {
          puts("face_diag=unavailable");
          return EXIT_FAILURE;
        }

      fputs(output, stdout);
      return EXIT_SUCCESS;
#else
      puts("face_diag=unavailable");
      return EXIT_FAILURE;
#endif
    }

  if (argc > 1 && strcmp(argv[1], "command") == 0)
    {
      if (argc != 3)
        {
          fprintf(stderr, "usage: agentguard command <phrase>\n");
          return EXIT_FAILURE;
        }

      printf("command=%d\n", (int)ag_parse_command(argv[2]));
      return EXIT_SUCCESS;
    }

  return ag_run();
}
