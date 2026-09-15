/* SPDX-License-Identifier: Apache-2.0 */

#ifndef AGENTGUARD_DISPLAY_UI_H
#define AGENTGUARD_DISPLAY_UI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define AG_UI_HEADER_HEIGHT 18
#define AG_UI_FOOTER_HEIGHT 34

struct ag_ui_status
{
  uint64_t seated_ms;
  uint32_t frame_sequence;
  uint8_t face_count;
  uint8_t posture_score;
  uint8_t camera_phase;
  uint8_t msr_candidates;
  int16_t model_input_min;
  int16_t model_input_max;
  uint8_t msr_score_percent;
  uint8_t pixel_mode_scores[4];
  uint8_t reference_score_percent;
  uint8_t reference_face_count;
  uint8_t tie_selftest_stage;
  bool tie_selftest_valid;
  bool tie_ram_pass;
  bool tie_flash_pass;
  uint32_t inference_ms;
  uint32_t capture_interval_ms;
  uint32_t dequeue_wait_ms;
  uint32_t loop_interval_ms;
  uint32_t lcd_write_ms;
  uint32_t lcd_submit_sum_ms;
  uint32_t lcd_submit_max_ms;
  uint32_t face_diagnostics_generation;
  uint8_t face_diagnostics_mnp_attempts;
  uint8_t face_diagnostics_mnp_accepted;
  uint8_t face_diagnostics_faces;
  bool face_diagnostics_valid;
  bool face_diagnostics_raw_change_valid;
  bool face_diagnostics_raw_changed;
  bool face_diagnostics_input_change_valid;
  bool face_diagnostics_input_changed;
  bool model_diagnostics_valid;
  bool frame_timing_valid;
  bool lcd_write_valid;
  bool lcd_submit_timing_valid;
  bool calibrated;
  bool reminders_paused;
  bool privacy_enabled;
  bool sedentary_alerted;
  bool posture_alerted;
  bool awaiting_ack;
  bool activity_on;
  bool camera_stale;
  bool ai_error;
  bool acknowledged;
};

void ag_ui_format_face_count(char *buffer, size_t buffer_size,
                             const struct ag_ui_status *status);
const char *ag_ui_primary_status(const struct ag_ui_status *status,
                                 uint16_t *color);
void ag_ui_format_model_diagnostics(char *buffer, size_t buffer_size,
                                    uint8_t msr_candidates,
                                    uint32_t inference_ms,
                                    bool tie_selftest_valid,
                                    bool tie_ram_pass,
                                    bool tie_flash_pass);
void ag_ui_format_frame_timing(char *buffer, size_t buffer_size,
                               uint32_t capture_interval_ms,
                               uint32_t dequeue_wait_ms,
                               uint32_t loop_interval_ms,
                               uint32_t inference_ms);
void ag_ui_format_lcd_timing(char *buffer, size_t buffer_size,
                             uint32_t capture_interval_ms,
                             uint32_t dequeue_wait_ms,
                             uint32_t loop_interval_ms,
                             uint32_t lcd_write_ms);
void ag_ui_format_camera_phase(char *buffer, size_t buffer_size,
                               uint8_t phase);
void ag_ui_format_tie_selftest(char *buffer, size_t buffer_size,
                               bool valid, bool ram_pass, bool flash_pass);
void ag_ui_format_tie_progress(char *buffer, size_t buffer_size,
                               uint8_t stage, bool valid,
                               bool ram_pass, bool flash_pass);
void ag_ui_format_face_diagnostics(
  char *buffer, size_t buffer_size, uint32_t generation,
  bool raw_change_valid, bool raw_changed,
  bool input_change_valid, bool input_changed,
  uint8_t mnp_attempts, uint8_t mnp_accepted, uint8_t faces);
bool ag_ui_format_diagnostic_detail(char *buffer, size_t buffer_size,
                                    const struct ag_ui_status *status);
void ag_ui_format_model_signal(char *buffer, size_t buffer_size,
                               int16_t input_min, int16_t input_max,
                               uint8_t score_percent);
void ag_ui_format_pixel_mode_scores(char *buffer, size_t buffer_size,
                                    const uint8_t scores[4]);
void ag_ui_format_reference_result(char *buffer, size_t buffer_size,
                                   uint8_t reference_score_percent,
                                   uint8_t reference_face_count,
                                   uint8_t live_score_percent);
uint16_t ag_ui_face_color(const struct ag_ui_status *status);
void ag_ui_rotate_180_rgb565(uint16_t *pixels, uint16_t width,
                             uint16_t height);
void ag_ui_render_rgb565(uint16_t *pixels, uint16_t frame_width,
                         uint16_t frame_height, uint16_t view_width,
                         uint16_t view_height,
                         const struct ag_ui_status *status);
void ag_ui_render_oriented_rgb565(uint16_t *pixels, uint16_t frame_width,
                                  uint16_t frame_height,
                                  uint16_t view_width,
                                  uint16_t view_height,
                                  const struct ag_ui_status *status);

#endif
