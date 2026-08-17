/* SPDX-License-Identifier: Apache-2.0 */

#include "agentguard/display_ui.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define AG_UI_BLACK       0x0000
#define AG_UI_WHITE       0xffff
#define AG_UI_GREEN       0x07e0
#define AG_UI_YELLOW      0xffe0
#define AG_UI_ORANGE      0xfd20
#define AG_UI_RED         0xf800
#define AG_UI_MAGENTA     0xf81f
#define AG_UI_CYAN        0x07ff
#define AG_UI_DARK_BLUE   0x0861
#define AG_UI_DARK_GRAY   0x2104
#define AG_UI_HEADER_H    18
#define AG_UI_FOOTER_H    34

static const uint8_t *ag_ui_glyph(char character)
{
  static const uint8_t blank[5] = {0, 0, 0, 0, 0};
  static const uint8_t colon[5] = {0, 0x14, 0, 0, 0};
  static const uint8_t dash[5] = {0x08, 0x08, 0x08, 0x08, 0x08};
  static const uint8_t plus[5] = {0x08, 0x08, 0x3e, 0x08, 0x08};
  static const uint8_t digits[10][5] =
  {
    {0x3e, 0x51, 0x49, 0x45, 0x3e},
    {0x00, 0x42, 0x7f, 0x40, 0x00},
    {0x42, 0x61, 0x51, 0x49, 0x46},
    {0x21, 0x41, 0x45, 0x4b, 0x31},
    {0x18, 0x14, 0x12, 0x7f, 0x10},
    {0x27, 0x45, 0x45, 0x45, 0x39},
    {0x3c, 0x4a, 0x49, 0x49, 0x30},
    {0x01, 0x71, 0x09, 0x05, 0x03},
    {0x36, 0x49, 0x49, 0x49, 0x36},
    {0x06, 0x49, 0x49, 0x29, 0x1e}
  };
  static const uint8_t letters[26][5] =
  {
    {0x7e, 0x11, 0x11, 0x11, 0x7e},
    {0x7f, 0x49, 0x49, 0x49, 0x36},
    {0x3e, 0x41, 0x41, 0x41, 0x22},
    {0x7f, 0x41, 0x41, 0x22, 0x1c},
    {0x7f, 0x49, 0x49, 0x49, 0x41},
    {0x7f, 0x09, 0x09, 0x09, 0x01},
    {0x3e, 0x41, 0x49, 0x49, 0x7a},
    {0x7f, 0x08, 0x08, 0x08, 0x7f},
    {0x00, 0x41, 0x7f, 0x41, 0x00},
    {0x20, 0x40, 0x41, 0x3f, 0x01},
    {0x7f, 0x08, 0x14, 0x22, 0x41},
    {0x7f, 0x40, 0x40, 0x40, 0x40},
    {0x7f, 0x02, 0x0c, 0x02, 0x7f},
    {0x7f, 0x04, 0x08, 0x10, 0x7f},
    {0x3e, 0x41, 0x41, 0x41, 0x3e},
    {0x7f, 0x09, 0x09, 0x09, 0x06},
    {0x3e, 0x41, 0x51, 0x21, 0x5e},
    {0x7f, 0x09, 0x19, 0x29, 0x46},
    {0x46, 0x49, 0x49, 0x49, 0x31},
    {0x01, 0x01, 0x7f, 0x01, 0x01},
    {0x3f, 0x40, 0x40, 0x40, 0x3f},
    {0x1f, 0x20, 0x40, 0x20, 0x1f},
    {0x3f, 0x40, 0x38, 0x40, 0x3f},
    {0x63, 0x14, 0x08, 0x14, 0x63},
    {0x07, 0x08, 0x70, 0x08, 0x07},
    {0x61, 0x51, 0x49, 0x45, 0x43}
  };

  if (character >= '0' && character <= '9')
    {
      return digits[character - '0'];
    }

  if (character >= 'A' && character <= 'Z')
    {
      return letters[character - 'A'];
    }

  if (character == ':') return colon;
  if (character == '-') return dash;
  if (character == '+') return plus;
  return blank;
}

static void ag_ui_format_bounded_ms(char *buffer, size_t buffer_size,
                                    uint32_t value)
{
  if (value > 9999)
    {
      snprintf(buffer, buffer_size, "9999+");
    }
  else
    {
      snprintf(buffer, buffer_size, "%lu", (unsigned long)value);
    }
}

void ag_ui_format_model_diagnostics(char *buffer, size_t buffer_size,
                                    uint8_t msr_candidates,
                                    uint32_t inference_ms,
                                    bool tie_selftest_valid,
                                    bool tie_ram_pass,
                                    bool tie_flash_pass)
{
  char model_text[24];

  if (buffer == NULL || buffer_size == 0)
    {
      return;
    }

  if (msr_candidates > 99 && inference_ms > 9999)
    {
      snprintf(model_text, sizeof(model_text), "M:99+ AI:9999+");
    }
  else if (msr_candidates > 99)
    {
      snprintf(model_text, sizeof(model_text), "M:99+ AI:%lu",
               (unsigned long)inference_ms);
    }
  else if (inference_ms > 9999)
    {
      snprintf(model_text, sizeof(model_text), "M:%u AI:9999+",
               msr_candidates);
    }
  else
    {
      snprintf(model_text, sizeof(model_text), "M:%u AI:%lu",
               msr_candidates,
               (unsigned long)inference_ms);
    }

  if (!tie_selftest_valid)
    {
      snprintf(buffer, buffer_size, "%s K:--", model_text);
      return;
    }

  snprintf(buffer, buffer_size, "%s K:%u%u", model_text,
           tie_ram_pass ? 1 : 0, tie_flash_pass ? 1 : 0);
}

void ag_ui_format_frame_timing(char *buffer, size_t buffer_size,
                               uint32_t capture_interval_ms,
                               uint32_t dequeue_wait_ms,
                               uint32_t loop_interval_ms,
                               uint32_t inference_ms)
{
  char capture_text[6];
  char dequeue_text[6];
  char loop_text[6];
  char inference_text[6];

  if (buffer == NULL || buffer_size == 0)
    {
      return;
    }

  ag_ui_format_bounded_ms(capture_text, sizeof(capture_text),
                          capture_interval_ms);
  ag_ui_format_bounded_ms(dequeue_text, sizeof(dequeue_text),
                          dequeue_wait_ms);
  ag_ui_format_bounded_ms(loop_text, sizeof(loop_text), loop_interval_ms);
  ag_ui_format_bounded_ms(inference_text, sizeof(inference_text),
                          inference_ms);
  snprintf(buffer, buffer_size, "C:%s Q:%s L:%s AI:%s",
           capture_text, dequeue_text, loop_text, inference_text);
}

void ag_ui_format_lcd_timing(char *buffer, size_t buffer_size,
                             uint32_t capture_interval_ms,
                             uint32_t dequeue_wait_ms,
                             uint32_t loop_interval_ms,
                             uint32_t lcd_write_ms)
{
  char capture_text[6];
  char dequeue_text[6];
  char loop_text[6];
  char display_text[6];

  if (buffer == NULL || buffer_size == 0)
    {
      return;
    }

  ag_ui_format_bounded_ms(capture_text, sizeof(capture_text),
                          capture_interval_ms);
  ag_ui_format_bounded_ms(dequeue_text, sizeof(dequeue_text),
                          dequeue_wait_ms);
  ag_ui_format_bounded_ms(loop_text, sizeof(loop_text), loop_interval_ms);
  ag_ui_format_bounded_ms(display_text, sizeof(display_text), lcd_write_ms);
  snprintf(buffer, buffer_size, "C:%s Q:%s L:%s D:%s",
           capture_text, dequeue_text, loop_text, display_text);
}

void ag_ui_format_camera_phase(char *buffer, size_t buffer_size,
                               uint8_t phase)
{
  if (buffer == NULL || buffer_size == 0)
    {
      return;
    }

  snprintf(buffer, buffer_size, "CAM:%u", phase);
}

void ag_ui_format_tie_selftest(char *buffer, size_t buffer_size,
                               bool valid, bool ram_pass, bool flash_pass)
{
  if (buffer == NULL || buffer_size == 0)
    {
      return;
    }

  if (!valid)
    {
      snprintf(buffer, buffer_size, "K:--");
      return;
    }

  snprintf(buffer, buffer_size, "K:%u%u",
           ram_pass ? 1 : 0, flash_pass ? 1 : 0);
}

void ag_ui_format_tie_progress(char *buffer, size_t buffer_size,
                               uint8_t stage, bool valid,
                               bool ram_pass, bool flash_pass)
{
  if (buffer == NULL || buffer_size == 0)
    {
      return;
    }

  if (stage == 0)
    {
      ag_ui_format_tie_selftest(buffer, buffer_size, valid,
                                ram_pass, flash_pass);
    }
  else if (stage == 42 && valid)
    {
      snprintf(buffer, buffer_size, "D:42 K:%u%u",
               ram_pass ? 1 : 0, flash_pass ? 1 : 0);
    }
  else
    {
      snprintf(buffer, buffer_size, "D:%u", stage);
    }
}

bool ag_ui_format_diagnostic_detail(char *buffer, size_t buffer_size,
                                    const struct ag_ui_status *status)
{
  if (buffer == NULL || buffer_size == 0 || status == NULL)
    {
      return false;
    }

  if (status->frame_timing_valid && status->model_diagnostics_valid &&
      status->lcd_write_valid)
    {
      ag_ui_format_lcd_timing(buffer, buffer_size,
                              status->capture_interval_ms,
                              status->dequeue_wait_ms,
                              status->loop_interval_ms,
                              status->lcd_write_ms);
      return true;
    }

  if (status->frame_timing_valid && status->model_diagnostics_valid)
    {
      ag_ui_format_frame_timing(buffer, buffer_size,
                                status->capture_interval_ms,
                                status->dequeue_wait_ms,
                                status->loop_interval_ms,
                                status->inference_ms);
      return true;
    }

  if (status->model_diagnostics_valid)
    {
      ag_ui_format_model_diagnostics(buffer, buffer_size,
                                     status->msr_candidates,
                                     status->inference_ms,
                                     status->tie_selftest_valid,
                                     status->tie_ram_pass,
                                     status->tie_flash_pass);
      return true;
    }

  if (status->tie_selftest_stage != 0 || status->tie_selftest_valid)
    {
      ag_ui_format_tie_progress(buffer, buffer_size,
                                status->tie_selftest_stage,
                                status->tie_selftest_valid,
                                status->tie_ram_pass,
                                status->tie_flash_pass);
      return true;
    }

  return false;
}

void ag_ui_format_model_signal(char *buffer, size_t buffer_size,
                               int16_t input_min, int16_t input_max,
                               uint8_t score_percent)
{
  if (buffer == NULL || buffer_size == 0)
    {
      return;
    }

  snprintf(buffer, buffer_size, "IN:%d:%d S:%u", input_min, input_max,
           score_percent > 99 ? 99 : score_percent);
}

void ag_ui_format_pixel_mode_scores(char *buffer, size_t buffer_size,
                                    const uint8_t scores[4])
{
  if (buffer == NULL || buffer_size == 0 || scores == NULL)
    {
      return;
    }

  snprintf(buffer, buffer_size, "Q:%02u/%02u/%02u/%02u",
           scores[0] > 99 ? 99 : scores[0],
           scores[1] > 99 ? 99 : scores[1],
           scores[2] > 99 ? 99 : scores[2],
           scores[3] > 99 ? 99 : scores[3]);
}

void ag_ui_format_reference_result(char *buffer, size_t buffer_size,
                                   uint8_t reference_score_percent,
                                   uint8_t reference_face_count,
                                   uint8_t live_score_percent)
{
  if (buffer == NULL || buffer_size == 0)
    {
      return;
    }

  snprintf(buffer, buffer_size, "T:%u/F:%u S:%u",
           reference_score_percent > 99 ? 99 : reference_score_percent,
           reference_face_count > 9 ? 9 : reference_face_count,
           live_score_percent > 99 ? 99 : live_score_percent);
}

static void ag_ui_fill(uint16_t *pixels, uint16_t stride, unsigned int x,
                       unsigned int y, unsigned int width,
                       unsigned int height, uint16_t color)
{
  unsigned int row;

  for (row = 0; row < height; row++)
    {
      unsigned int column;
      uint16_t *line = pixels + (y + row) * stride + x;

      for (column = 0; column < width; column++)
        {
          line[column] = color;
        }
    }
}

static void ag_ui_text(uint16_t *pixels, uint16_t stride, unsigned int x,
                       unsigned int y, const char *text, uint16_t color)
{
  while (*text != '\0')
    {
      const uint8_t *glyph = ag_ui_glyph(*text++);
      unsigned int column;

      for (column = 0; column < 5; column++)
        {
          unsigned int row;

          for (row = 0; row < 7; row++)
            {
              if ((glyph[column] & (1u << row)) != 0)
                {
                  pixels[(y + row) * stride + x + column] = color;
                }
            }
        }

      x += 6;
    }
}

uint16_t ag_ui_face_color(const struct ag_ui_status *status)
{
  if (status == NULL || status->face_count == 0)
    {
      return AG_UI_RED;
    }

  if (status->face_count > 1 || status->privacy_enabled)
    {
      return AG_UI_MAGENTA;
    }

  if (status->posture_alerted || status->sedentary_alerted ||
      status->awaiting_ack)
    {
      return AG_UI_ORANGE;
    }

  return AG_UI_GREEN;
}

void ag_ui_rotate_180_rgb565(uint16_t *pixels, uint16_t width,
                             uint16_t height)
{
  size_t left;
  size_t right;

  if (pixels == NULL || width == 0 || height == 0)
    {
      return;
    }

  left = 0;
  right = (size_t)width * height - 1;
  while (left < right)
    {
      uint16_t value = pixels[left];
      pixels[left++] = pixels[right];
      pixels[right--] = value;
    }
}

static const char *ag_ui_primary_status(const struct ag_ui_status *status,
                                        uint16_t *color)
{
  if (status->reminders_paused)
    {
      *color = AG_UI_CYAN;
      return "PAUSED";
    }

  if (status->face_count == 0)
    {
      *color = AG_UI_RED;
      return "NO PERSON";
    }

  if (status->face_count > 1)
    {
      *color = AG_UI_MAGENTA;
      return "MULTI PERSON";
    }

  if (status->awaiting_ack)
    {
      *color = AG_UI_ORANGE;
      return "PRESS BOOT TO ACK";
    }

  if (status->posture_alerted)
    {
      *color = AG_UI_ORANGE;
      return "POSTURE ALERT";
    }

  if (status->sedentary_alerted)
    {
      *color = AG_UI_ORANGE;
      return "STAND UP";
    }

  if (!status->calibrated)
    {
      *color = AG_UI_YELLOW;
      return "CALIBRATING";
    }

  *color = AG_UI_GREEN;
  return "MONITORING";
}

void ag_ui_render_rgb565(uint16_t *pixels, uint16_t frame_width,
                         uint16_t frame_height, uint16_t view_width,
                         uint16_t view_height,
                         const struct ag_ui_status *status)
{
  char face_text[16];
  char posture_text[16];
  char diagnostics_text[24];
  char detail_text[40];
  char camera_text[16];
  const char *primary;
  uint16_t accent;
  unsigned int origin_x;
  unsigned int origin_y;
  unsigned int footer_y;
  unsigned long total_seconds;
  unsigned long minutes;
  unsigned long seconds;

  if (pixels == NULL || status == NULL || view_width < 120 ||
      view_height < AG_UI_HEADER_H + AG_UI_FOOTER_H ||
      view_width > frame_width || view_height > frame_height)
    {
      return;
    }

  origin_x = (frame_width - view_width) / 2;
  origin_y = (frame_height - view_height) / 2;
  footer_y = origin_y + view_height - AG_UI_FOOTER_H;
  accent = ag_ui_face_color(status);

  ag_ui_fill(pixels, frame_width, origin_x, origin_y, view_width,
             AG_UI_HEADER_H, AG_UI_DARK_BLUE);
  ag_ui_fill(pixels, frame_width, origin_x, footer_y, view_width,
             AG_UI_FOOTER_H, AG_UI_DARK_GRAY);
  ag_ui_fill(pixels, frame_width, origin_x + 4, origin_y + 4, 10, 10,
             accent);
  if (status->activity_on)
    {
      ag_ui_fill(pixels, frame_width, origin_x + 7, origin_y + 7, 4, 4,
                 AG_UI_WHITE);
    }

  if (status->face_count > 9)
    {
      snprintf(face_text, sizeof(face_text), "FACE:9+");
    }
  else
    {
      snprintf(face_text, sizeof(face_text), "FACE:%u",
               status->face_count);
    }

  ag_ui_text(pixels, frame_width, origin_x + 19, origin_y + 5,
             face_text, AG_UI_WHITE);
  if (status->model_diagnostics_valid)
    {
      ag_ui_format_reference_result(diagnostics_text,
                                    sizeof(diagnostics_text),
                                    status->reference_score_percent,
                                    status->reference_face_count,
                                    status->msr_score_percent);
      ag_ui_text(pixels, frame_width, origin_x + view_width - 90,
                 origin_y + 5, diagnostics_text, AG_UI_WHITE);
    }
  else
    {
      snprintf(posture_text, sizeof(posture_text), "POST:%u",
               status->posture_score);
      ag_ui_text(pixels, frame_width, origin_x + view_width - 54,
                 origin_y + 5, posture_text, AG_UI_WHITE);
    }

  if (status->camera_stale)
    {
      accent = AG_UI_RED;
      ag_ui_format_camera_phase(camera_text, sizeof(camera_text),
                                status->camera_phase);
      primary = camera_text;
    }
  else
    {
      primary = ag_ui_primary_status(status, &accent);
    }
  ag_ui_text(pixels, frame_width, origin_x + 5, footer_y + 5,
             primary, accent);

  total_seconds = (unsigned long)(status->seated_ms / 1000);
  minutes = total_seconds / 60;
  seconds = total_seconds % 60;
  if (minutes > 99) minutes = 99;
  if (!ag_ui_format_diagnostic_detail(detail_text, sizeof(detail_text),
                                      status))
    {
      snprintf(detail_text, sizeof(detail_text), "SIT:%02lu:%02lu FRM:%05lu",
               minutes, seconds,
               (unsigned long)(status->frame_sequence % 100000));
    }
  ag_ui_text(pixels, frame_width, origin_x + 5, footer_y + 19,
             detail_text, status->privacy_enabled ? AG_UI_MAGENTA :
             AG_UI_WHITE);
}
