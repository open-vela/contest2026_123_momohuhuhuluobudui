/* SPDX-License-Identifier: Apache-2.0 */

#include "agentguard/display_ui.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define WIDTH 320
#define HEIGHT 240
#define VIEW_WIDTH 240
#define VIEW_X 40

static uint16_t frame[WIDTH * HEIGHT];

int main(void)
{
  struct ag_ui_status status;
  char diagnostics[24];
  uint16_t rotation_sample[6] = {1, 2, 3, 4, 5, 6};
  uint16_t no_face_color;
  uint16_t one_face_color;
  uint16_t multiple_color;

  ag_ui_format_model_diagnostics(diagnostics, sizeof(diagnostics), 3, 847,
                                 false, false, false);
  assert(strcmp(diagnostics, "M:3 AI:847 K:--") == 0);

  ag_ui_format_model_diagnostics(diagnostics, sizeof(diagnostics), 3, 847,
                                 true, true, false);
  assert(strcmp(diagnostics, "M:3 AI:847 K:10") == 0);

  ag_ui_format_model_diagnostics(diagnostics, sizeof(diagnostics), 255,
                                 123456, true, true, true);
  assert(strcmp(diagnostics, "M:99+ AI:9999+ K:11") == 0);

  ag_ui_format_model_signal(diagnostics, sizeof(diagnostics), -128, 127,
                            50);
  assert(strcmp(diagnostics, "IN:-128:127 S:50") == 0);

  {
    const uint8_t scores[4] = {2, 47, 100, 3};
    ag_ui_format_pixel_mode_scores(diagnostics, sizeof(diagnostics), scores);
    assert(strcmp(diagnostics, "Q:02/47/99/03") == 0);
  }

  ag_ui_format_reference_result(diagnostics, sizeof(diagnostics), 87, 1,
                                42);
  assert(strcmp(diagnostics, "T:87/F:1 S:42") == 0);

  ag_ui_rotate_180_rgb565(rotation_sample, 3, 2);
  assert(rotation_sample[0] == 6);
  assert(rotation_sample[1] == 5);
  assert(rotation_sample[2] == 4);
  assert(rotation_sample[3] == 3);
  assert(rotation_sample[4] == 2);
  assert(rotation_sample[5] == 1);

  memset(&status, 0, sizeof(status));
  memset(frame, 0x55, sizeof(frame));
  ag_ui_render_rgb565(frame, WIDTH, HEIGHT, VIEW_WIDTH, HEIGHT, &status);
  no_face_color = frame[4 * WIDTH + VIEW_X + 4];
  assert(no_face_color == ag_ui_face_color(&status));
  assert(frame[100 * WIDTH + 160] == 0x5555);

  status.face_count = 1;
  status.calibrated = true;
  status.seated_ms = 125000;
  status.frame_sequence = 12345;
  one_face_color = ag_ui_face_color(&status);
  assert(one_face_color != no_face_color);
  ag_ui_render_rgb565(frame, WIDTH, HEIGHT, VIEW_WIDTH, HEIGHT, &status);
  assert(frame[4 * WIDTH + VIEW_X + 4] == one_face_color);

  status.activity_on = true;
  ag_ui_render_rgb565(frame, WIDTH, HEIGHT, VIEW_WIDTH, HEIGHT, &status);
  assert(frame[7 * WIDTH + VIEW_X + 7] == 0xffff);
  assert(frame[4 * WIDTH + VIEW_X + 4] == one_face_color);

  status.face_count = 2;
  status.privacy_enabled = true;
  multiple_color = ag_ui_face_color(&status);
  assert(multiple_color != one_face_color);

  status.face_count = 1;
  status.privacy_enabled = false;
  status.posture_alerted = true;
  assert(ag_ui_face_color(&status) != one_face_color);

  status.camera_stale = true;
  ag_ui_render_rgb565(frame, WIDTH, HEIGHT, VIEW_WIDTH, HEIGHT, &status);
  assert(frame[(HEIGHT - 34 + 5) * WIDTH + VIEW_X + 5] != 0x5555);

  puts("AgentGuard display UI tests: PASS");
  return 0;
}
