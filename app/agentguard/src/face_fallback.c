/* SPDX-License-Identifier: Apache-2.0 */

#include "agentguard/face_fallback.h"

#include <stddef.h>
#include <string.h>

#define AG_FACE_GRID_STEP 4
#define AG_FACE_GRID_MAX_WIDTH 80
#define AG_FACE_GRID_MAX_HEIGHT 60
#define AG_FACE_GRID_MAX_PIXELS \
  (AG_FACE_GRID_MAX_WIDTH * AG_FACE_GRID_MAX_HEIGHT)
#define AG_FACE_MIN_CELLS 40

static uint8_t g_skin[AG_FACE_GRID_MAX_PIXELS];
static uint8_t g_seen[AG_FACE_GRID_MAX_PIXELS];
static uint16_t g_queue[AG_FACE_GRID_MAX_PIXELS];

static bool ag_face_is_skin(uint16_t rgb565)
{
  int r = (((rgb565 >> 11) & 0x1f) * 255) / 31;
  int g = (((rgb565 >> 5) & 0x3f) * 255) / 63;
  int b = ((rgb565 & 0x1f) * 255) / 31;
  int maximum = r > g ? r : g;
  int minimum = r < g ? r : g;

  maximum = maximum > b ? maximum : b;
  minimum = minimum < b ? minimum : b;
  return r > 75 && g > 35 && b > 20 && r > g && r > b &&
         maximum - minimum > 15 && r - g > 8;
}

bool ag_face_fallback_apply_rgb565(const uint16_t *pixels,
                                   uint16_t width, uint16_t height,
                                   struct ag_vision_result *result)
{
  unsigned int grid_width;
  unsigned int grid_height;
  unsigned int grid_size;
  unsigned int largest = 0;
  struct ag_face_box primary = {0};
  unsigned int x;
  unsigned int y;
  unsigned int i;

  if (pixels == NULL || result == NULL || result->face_count != 0 ||
      width < AG_FACE_GRID_STEP || height < AG_FACE_GRID_STEP)
    {
      return false;
    }

  grid_width = width / AG_FACE_GRID_STEP;
  grid_height = height / AG_FACE_GRID_STEP;
  if (grid_width > AG_FACE_GRID_MAX_WIDTH ||
      grid_height > AG_FACE_GRID_MAX_HEIGHT)
    {
      return false;
    }

  grid_size = grid_width * grid_height;
  memset(g_skin, 0, grid_size);
  memset(g_seen, 0, grid_size);
  for (y = 0; y < grid_height; y++)
    {
      for (x = 0; x < grid_width; x++)
        {
          g_skin[y * grid_width + x] =
            ag_face_is_skin(pixels[(y * AG_FACE_GRID_STEP) * width +
                                   x * AG_FACE_GRID_STEP]);
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
      unsigned int cells_width;
      unsigned int cells_height;

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
          static const int dx[8] = {-1, 1, 0, 0, -1, -1, 1, 1};
          static const int dy[8] = {0, 0, -1, 1, -1, 1, -1, 1};
          unsigned int current = g_queue[head++];
          unsigned int cx = current % grid_width;
          unsigned int cy = current / grid_width;
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

      cells_width = max_x - min_x + 1;
      cells_height = max_y - min_y + 1;
      if (tail < AG_FACE_MIN_CELLS || cells_width < 4 ||
          cells_height < 4 || cells_width * 2 < cells_height ||
          cells_height * 2 < cells_width)
        {
          continue;
        }

      if (tail > largest)
        {
          largest = tail;
          primary.x = min_x * AG_FACE_GRID_STEP;
          primary.y = min_y * AG_FACE_GRID_STEP;
          primary.width = cells_width * AG_FACE_GRID_STEP;
          primary.height = cells_height * AG_FACE_GRID_STEP;
        }
    }

  if (largest == 0)
    {
      return false;
    }

  result->face_count = 1;
  result->face_box_count = 1;
  result->posture_score = 0;
  result->primary_face = primary;
  result->face_boxes[0] = primary;
  return true;
}
