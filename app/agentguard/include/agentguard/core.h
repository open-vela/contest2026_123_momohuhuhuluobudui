/* SPDX-License-Identifier: Apache-2.0 */

#ifndef AGENTGUARD_CORE_H
#define AGENTGUARD_CORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum ag_event
{
  AG_EVENT_NONE             = 0,
  AG_EVENT_PRESENCE_STARTED = 1u << 0,
  AG_EVENT_BREAK_STARTED    = 1u << 1,
  AG_EVENT_SEDENTARY_ALERT  = 1u << 2,
  AG_EVENT_POSTURE_ALERT    = 1u << 3,
  AG_EVENT_PRIVACY_ON       = 1u << 4,
  AG_EVENT_PRIVACY_OFF      = 1u << 5,
  AG_EVENT_BLUR_SCREEN      = 1u << 6,
  AG_EVENT_UNBLUR_SCREEN    = 1u << 7,
  AG_EVENT_LOCK_SCREEN      = 1u << 8,
  AG_EVENT_REMINDERS_PAUSED = 1u << 9,
  AG_EVENT_REMINDERS_ACTIVE = 1u << 10,
  AG_EVENT_ACKNOWLEDGED     = 1u << 11,
  AG_EVENT_BRIGHTNESS_UP    = 1u << 12,
  AG_EVENT_BRIGHTNESS_DOWN  = 1u << 13,
  AG_EVENT_AI_ERROR         = 1u << 14,
  AG_EVENT_AI_RECOVERED     = 1u << 15,
};

enum ag_command
{
  AG_COMMAND_NONE = 0,
  AG_COMMAND_ACKNOWLEDGE,
  AG_COMMAND_PAUSE,
  AG_COMMAND_RESUME,
  AG_COMMAND_PRIVACY_ON,
  AG_COMMAND_PRIVACY_OFF,
  AG_COMMAND_BRIGHTNESS_UP,
  AG_COMMAND_BRIGHTNESS_DOWN,
};

struct ag_config
{
  uint64_t sedentary_ms;
  uint64_t reminder_grace_ms;
  uint64_t break_confirm_ms;
  uint64_t privacy_absence_lock_ms;
  uint64_t posture_hold_ms;
  uint8_t posture_score_limit;
  bool lock_after_unanswered_reminder;
  bool lock_when_privacy_user_leaves;
};

struct ag_observation
{
  uint64_t monotonic_ms;
  uint8_t face_count;
  uint8_t posture_score;
  enum ag_command command;
  bool vision_valid;
};

struct ag_state
{
  bool initialized;
  bool present;
  bool reminders_paused;
  bool privacy_enabled;
  bool privacy_blurred;
  bool sedentary_alerted;
  bool posture_alerted;
  bool awaiting_ack;
  bool lock_sent;
  bool vision_paused;
  bool vision_ack_pending;
  uint64_t presence_since_ms;
  uint64_t absence_since_ms;
  uint64_t poor_posture_since_ms;
  uint64_t awaiting_ack_since_ms;
  uint64_t vision_paused_since_ms;
};

void ag_default_config(struct ag_config *config);
void ag_set_demo_mode(struct ag_state *state, struct ag_config *config,
                      bool enabled, uint64_t now_ms);
bool ag_attention_led_enabled(const struct ag_state *state);
void ag_init(struct ag_state *state);
uint32_t ag_step(struct ag_state *state, const struct ag_config *config,
                 const struct ag_observation *observation);
enum ag_command ag_parse_command(const char *text);
const char *ag_event_name(enum ag_event event);
int ag_event_json(char *buffer, size_t size, enum ag_event event,
                  uint64_t monotonic_ms, const struct ag_state *state);

#ifdef __cplusplus
}
#endif

#endif
