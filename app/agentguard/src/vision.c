/* SPDX-License-Identifier: Apache-2.0 */

#include "agentguard/vision.h"

#ifdef CONFIG_AGENTGUARD_ESP_DL
#  include "agentguard/vision_model.h"
#endif

#include <stdbool.h>
#include <string.h>

#define AG_GRID_STEP 4
#define AG_GRID_MAX_WIDTH 80
#define AG_GRID_MAX_HEIGHT 60
#define AG_GRID_MAX_PIXELS (AG_GRID_MAX_WIDTH * AG_GRID_MAX_HEIGHT)
#define AG_MIN_FACE_CELLS 40

static uint8_t g_skin[AG_GRID_MAX_PIXELS];
static uint8_t g_seen[AG_GRID_MAX_PIXELS];
static uint16_t g_queue[AG_GRID_MAX_PIXELS];

static bool ag_is_skin(uint16_t rgb565)
{
  int r5 = (rgb565 >> 11) & 0x1f;
  int g6 = (rgb565 >> 5) & 0x3f;
  int b5 = rgb565 & 0x1f;
  int r = (r5 * 255) / 31;
  int g = (g6 * 255) / 63;
  int b = (b5 * 255) / 31;
  int maximum = r > g ? r : g;
  int minimum;

  maximum = maximum > b ? maximum : b;
  minimum = r < g ? r : g;
  minimum = minimum < b ? minimum : b;

  return r > 75 && g > 35 && b > 20 && r > g && r > b &&
         maximum - minimum > 15 && r - g > 8;
}

void ag_vision_init(struct ag_vision_context *context)
{
  memset(context, 0, sizeof(*context));
}

static unsigned int ag_score_posture(struct ag_vision_context *context,
                                     const struct ag_face_box *face,
                                     uint16_t image_height)
{
  uint16_t center_y = face->y + face->height / 2;
  unsigned int score = 0;

  if (context->stable_frames < 20)
    {
      if (context->stable_frames == 0)
        {
          context->reference_center_y = center_y;
          context->reference_height = face->height;
        }
      else
        {
          context->reference_center_y =
            (context->reference_center_y * 7 + center_y) / 8;
          context->reference_height =
            (context->reference_height * 7 + face->height) / 8;
        }

      context->stable_frames++;
      return 0;
    }

  if (center_y > context->reference_center_y)
    {
      score += (center_y - context->reference_center_y) * 200 /
               image_height;
    }

  if (context->reference_height > 0 &&
      face->height > context->reference_height)
    {
      score += (face->height - context->reference_height) * 100 /
               context->reference_height;
    }

  return score > 100 ? 100 : score;
}

int ag_vision_process_rgb565(struct ag_vision_context *context,
                             const uint16_t *pixels, uint16_t width,
                             uint16_t height,
                             struct ag_vision_result *result)
{
  unsigned int grid_width;
  unsigned int grid_height;
  unsigned int grid_size;
  unsigned int x;
  unsigned int y;
  unsigned int i;
  unsigned int face_count = 0;
  unsigned int largest = 0;
  struct ag_face_box primary = {0};

  if (context == NULL || pixels == NULL || result == NULL ||
      width < AG_GRID_STEP || height < AG_GRID_STEP)
    {
      return -1;
    }

#ifdef CONFIG_AGENTGUARD_ESP_DL
  if (ag_vision_model_process_rgb565(pixels, width, height, result) < 0)
    {
      return -1;
    }

  if (result->face_count > 0)
    {
      result->posture_score =
        ag_score_posture(context, &result->primary_face, height);
    }

  return 0;
#endif

  grid_width = width / AG_GRID_STEP;
  grid_height = height / AG_GRID_STEP;
  if (grid_width > AG_GRID_MAX_WIDTH || grid_height > AG_GRID_MAX_HEIGHT)
    {
      return -1;
    }

  grid_size = grid_width * grid_height;
  memset(g_skin, 0, grid_size);
  memset(g_seen, 0, grid_size);
  memset(result, 0, sizeof(*result));

  for (y = 0; y < grid_height; y++)
    {
      for (x = 0; x < grid_width; x++)
        {
          g_skin[y * grid_width + x] =
            ag_is_skin(pixels[(y * AG_GRID_STEP) * width +
                              x * AG_GRID_STEP]);
        }
    }

  for (i = 0; i < grid_size; i++)
    {
      unsigned int head = 0;
      unsigned int tail = 0;
      unsigned int min_x;
      unsigned int max_x;
      unsigned int min_y;
      unsigned int max_y;

      if (!g_skin[i] || g_seen[i])
        {
          continue;
        }

      min_x = max_x = i % grid_width;
      min_y = max_y = i / grid_width;
      g_seen[i] = 1;
      g_queue[tail++] = i;

      while (head < tail)
        {
          unsigned int current = g_queue[head++];
          unsigned int cx = current % grid_width;
          unsigned int cy = current / grid_width;
          const int dx[8] = {-1, 1, 0, 0, -1, -1, 1, 1};
          const int dy[8] = {0, 0, -1, 1, -1, 1, -1, 1};
          unsigned int direction;

          if (cx < min_x) min_x = cx;
          if (cx > max_x) max_x = cx;
          if (cy < min_y) min_y = cy;
          if (cy > max_y) max_y = cy;

          for (direction = 0; direction < 8; direction++)
            {
              int nx = (int)cx + dx[direction];
              int ny = (int)cy + dy[direction];
              unsigned int next;

              if (nx < 0 || ny < 0 || nx >= (int)grid_width ||
                  ny >= (int)grid_height)
                {
                  continue;
                }

              next = (unsigned int)ny * grid_width + (unsigned int)nx;
              if (g_skin[next] && !g_seen[next])
                {
                  g_seen[next] = 1;
                  g_queue[tail++] = next;
                }
            }
        }

      if (tail >= AG_MIN_FACE_CELLS && max_x - min_x >= 3 &&
          max_y - min_y >= 3)
        {
          face_count++;
          if (tail > largest)
            {
              largest = tail;
              primary.x = min_x * AG_GRID_STEP;
              primary.y = min_y * AG_GRID_STEP;
              primary.width = (max_x - min_x + 1) * AG_GRID_STEP;
              primary.height = (max_y - min_y + 1) * AG_GRID_STEP;
            }
        }
    }

  result->face_count = face_count > 255 ? 255 : face_count;
  result->primary_face = primary;
  if (face_count > 0)
    {
      result->posture_score = ag_score_posture(context, &primary, height);
    }

  return 0;
}
