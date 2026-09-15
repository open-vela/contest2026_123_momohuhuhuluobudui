/* SPDX-License-Identifier: Apache-2.0 */

#include "agentguard/core.h"

#include <stdio.h>
#include <string.h>

static bool ag_elapsed(uint64_t now, uint64_t since, uint64_t duration)
{
  return now >= since && now - since >= duration;
}

static void ag_shift_timestamp(uint64_t *timestamp, uint64_t duration)
{
  if (*timestamp == 0)
    {
      return;
    }

  if (duration > UINT64_MAX - *timestamp)
    {
      *timestamp = UINT64_MAX;
    }
  else
    {
      *timestamp += duration;
    }
}

void ag_default_config(struct ag_config *config)
{
  memset(config, 0, sizeof(*config));
  config->sedentary_ms = 30ull * 60ull * 1000ull;
  config->reminder_grace_ms = 60ull * 1000ull;
  config->break_confirm_ms = 5ull * 60ull * 1000ull;
  config->privacy_absence_lock_ms = 15ull * 1000ull;
  config->posture_hold_ms = 10ull * 1000ull;
  config->posture_score_limit = 70;
  config->lock_after_unanswered_reminder = true;
  config->lock_when_privacy_user_leaves = true;
}

void ag_init(struct ag_state *state)
{
  memset(state, 0, sizeof(*state));
}

static uint32_t ag_apply_command(struct ag_state *state,
                                 enum ag_command command)
{
  uint32_t events = 0;

  switch (command)
    {
      case AG_COMMAND_ACKNOWLEDGE:
        state->awaiting_ack = false;
        state->sedentary_alerted = false;
        state->lock_sent = false;
        events |= AG_EVENT_ACKNOWLEDGED;
        break;

      case AG_COMMAND_PAUSE:
        if (!state->reminders_paused)
          {
            state->reminders_paused = true;
            events |= AG_EVENT_REMINDERS_PAUSED;
          }
        break;

      case AG_COMMAND_RESUME:
        if (state->reminders_paused)
          {
            state->reminders_paused = false;
            events |= AG_EVENT_REMINDERS_ACTIVE;
          }
        break;

      case AG_COMMAND_PRIVACY_ON:
        if (!state->privacy_enabled)
          {
            state->privacy_enabled = true;
            events |= AG_EVENT_PRIVACY_ON;
          }
        break;

      case AG_COMMAND_PRIVACY_OFF:
        if (state->privacy_enabled)
          {
            state->privacy_enabled = false;
            events |= AG_EVENT_PRIVACY_OFF;
            if (state->privacy_blurred)
              {
                state->privacy_blurred = false;
                events |= AG_EVENT_UNBLUR_SCREEN;
              }
          }
        break;

      case AG_COMMAND_BRIGHTNESS_UP:
        events |= AG_EVENT_BRIGHTNESS_UP;
        break;

      case AG_COMMAND_BRIGHTNESS_DOWN:
        events |= AG_EVENT_BRIGHTNESS_DOWN;
        break;

      default:
        break;
    }

  return events;
}

uint32_t ag_step(struct ag_state *state, const struct ag_config *config,
                 const struct ag_observation *observation)
{
  uint32_t events;
  uint64_t now = observation->monotonic_ms;
  bool has_face = observation->face_count > 0;

  events = ag_apply_command(state, observation->command);

  if (!observation->vision_valid)
    {
      if (!state->vision_paused)
        {
          state->vision_paused = true;
          state->vision_paused_since_ms = now;
        }

      if (observation->command == AG_COMMAND_ACKNOWLEDGE && state->present)
        {
          state->vision_ack_pending = true;
        }

      return events;
    }

  if (state->vision_paused)
    {
      uint64_t paused_ms = now >= state->vision_paused_since_ms ?
        now - state->vision_paused_since_ms : 0;

      ag_shift_timestamp(&state->presence_since_ms, paused_ms);
      ag_shift_timestamp(&state->absence_since_ms, paused_ms);
      ag_shift_timestamp(&state->poor_posture_since_ms, paused_ms);
      ag_shift_timestamp(&state->awaiting_ack_since_ms, paused_ms);
      state->vision_paused = false;
      state->vision_paused_since_ms = 0;
    }

  if (state->vision_ack_pending)
    {
      state->presence_since_ms = now;
      state->vision_ack_pending = false;
    }

  /* Acknowledging a reminder starts a fresh sitting interval.  Without this
   * reset the same observation could immediately raise a second alert. */

  if (observation->command == AG_COMMAND_ACKNOWLEDGE && has_face)
    {
      state->presence_since_ms = now;
    }

  if (!state->initialized)
    {
      state->initialized = true;
      state->present = has_face;
      if (has_face)
        {
          state->presence_since_ms = now;
          events |= AG_EVENT_PRESENCE_STARTED;
        }
      else
        {
          state->absence_since_ms = now;
        }
    }

  if (has_face)
    {
      if (!state->present)
        {
          state->present = true;
          state->presence_since_ms = now;
          state->sedentary_alerted = false;
          state->posture_alerted = false;
          state->awaiting_ack = false;
          state->lock_sent = false;
          events |= AG_EVENT_PRESENCE_STARTED;
        }

      state->absence_since_ms = 0;

      if (observation->posture_score >= config->posture_score_limit)
        {
          if (state->poor_posture_since_ms == 0)
            {
              state->poor_posture_since_ms = now;
            }
          else if (!state->posture_alerted && !state->reminders_paused &&
                   ag_elapsed(now, state->poor_posture_since_ms,
                              config->posture_hold_ms))
            {
              state->posture_alerted = true;
              events |= AG_EVENT_POSTURE_ALERT;
            }
        }
      else
        {
          state->poor_posture_since_ms = 0;
          state->posture_alerted = false;
        }

      if (!state->reminders_paused && !state->sedentary_alerted &&
          ag_elapsed(now, state->presence_since_ms, config->sedentary_ms))
        {
          state->sedentary_alerted = true;
          state->awaiting_ack = true;
          state->awaiting_ack_since_ms = now;
          events |= AG_EVENT_SEDENTARY_ALERT;
        }
    }
  else if (state->present)
    {
      if (state->absence_since_ms == 0)
        {
          state->absence_since_ms = now;
        }

      if (ag_elapsed(now, state->absence_since_ms, config->break_confirm_ms))
        {
          state->present = false;
          state->presence_since_ms = 0;
          state->poor_posture_since_ms = 0;
          state->sedentary_alerted = false;
          state->posture_alerted = false;
          state->awaiting_ack = false;
          state->lock_sent = false;
          events |= AG_EVENT_BREAK_STARTED;
        }
    }

  if (state->privacy_enabled && observation->face_count > 1)
    {
      if (!state->privacy_blurred)
        {
          state->privacy_blurred = true;
          events |= AG_EVENT_BLUR_SCREEN;
        }
    }
  else if (state->privacy_blurred)
    {
      state->privacy_blurred = false;
      events |= AG_EVENT_UNBLUR_SCREEN;
    }

  if (!state->lock_sent && config->lock_after_unanswered_reminder &&
      state->awaiting_ack &&
      ag_elapsed(now, state->awaiting_ack_since_ms,
                 config->reminder_grace_ms))
    {
      state->lock_sent = true;
      state->awaiting_ack = false;
      events |= AG_EVENT_LOCK_SCREEN;
    }

  if (!state->lock_sent && config->lock_when_privacy_user_leaves &&
      state->privacy_enabled && !has_face && state->absence_since_ms != 0 &&
      ag_elapsed(now, state->absence_since_ms,
                 config->privacy_absence_lock_ms))
    {
      state->lock_sent = true;
      events |= AG_EVENT_LOCK_SCREEN;
    }

  return events;
}

struct ag_phrase
{
  const char *text;
  enum ag_command command;
};

static const struct ag_phrase g_phrases[] =
{
  {"pause reminders", AG_COMMAND_PAUSE},
  {"stop reminders", AG_COMMAND_PAUSE},
  {"暂停提醒", AG_COMMAND_PAUSE},
  {"暂时关闭提醒", AG_COMMAND_PAUSE},
  {"resume reminders", AG_COMMAND_RESUME},
  {"start reminders", AG_COMMAND_RESUME},
  {"恢复提醒", AG_COMMAND_RESUME},
  {"继续提醒", AG_COMMAND_RESUME},
  {"privacy on", AG_COMMAND_PRIVACY_ON},
  {"enable privacy", AG_COMMAND_PRIVACY_ON},
  {"开启隐私模式", AG_COMMAND_PRIVACY_ON},
  {"打开隐私模式", AG_COMMAND_PRIVACY_ON},
  {"privacy off", AG_COMMAND_PRIVACY_OFF},
  {"disable privacy", AG_COMMAND_PRIVACY_OFF},
  {"关闭隐私模式", AG_COMMAND_PRIVACY_OFF},
  {"退出隐私模式", AG_COMMAND_PRIVACY_OFF},
  {"acknowledge", AG_COMMAND_ACKNOWLEDGE},
  {"I am standing", AG_COMMAND_ACKNOWLEDGE},
  {"我知道了", AG_COMMAND_ACKNOWLEDGE},
  {"我已起身", AG_COMMAND_ACKNOWLEDGE},
  {"brightness up", AG_COMMAND_BRIGHTNESS_UP},
  {"increase brightness", AG_COMMAND_BRIGHTNESS_UP},
  {"提高亮度", AG_COMMAND_BRIGHTNESS_UP},
  {"屏幕亮一点", AG_COMMAND_BRIGHTNESS_UP},
  {"brightness down", AG_COMMAND_BRIGHTNESS_DOWN},
  {"decrease brightness", AG_COMMAND_BRIGHTNESS_DOWN},
  {"降低亮度", AG_COMMAND_BRIGHTNESS_DOWN},
  {"屏幕暗一点", AG_COMMAND_BRIGHTNESS_DOWN},
};

enum ag_command ag_parse_command(const char *text)
{
  size_t i;

  if (text == NULL)
    {
      return AG_COMMAND_NONE;
    }

  for (i = 0; i < sizeof(g_phrases) / sizeof(g_phrases[0]); i++)
    {
      if (strcmp(text, g_phrases[i].text) == 0)
        {
          return g_phrases[i].command;
        }
    }

  return AG_COMMAND_NONE;
}

const char *ag_event_name(enum ag_event event)
{
  switch (event)
    {
      case AG_EVENT_PRESENCE_STARTED: return "presence_started";
      case AG_EVENT_BREAK_STARTED: return "break_started";
      case AG_EVENT_SEDENTARY_ALERT: return "sedentary_alert";
      case AG_EVENT_POSTURE_ALERT: return "posture_alert";
      case AG_EVENT_PRIVACY_ON: return "privacy_on";
      case AG_EVENT_PRIVACY_OFF: return "privacy_off";
      case AG_EVENT_BLUR_SCREEN: return "blur_screen";
      case AG_EVENT_UNBLUR_SCREEN: return "unblur_screen";
      case AG_EVENT_LOCK_SCREEN: return "lock_screen";
      case AG_EVENT_REMINDERS_PAUSED: return "reminders_paused";
      case AG_EVENT_REMINDERS_ACTIVE: return "reminders_active";
      case AG_EVENT_ACKNOWLEDGED: return "acknowledged";
      case AG_EVENT_BRIGHTNESS_UP: return "brightness_up";
      case AG_EVENT_BRIGHTNESS_DOWN: return "brightness_down";
      case AG_EVENT_AI_ERROR: return "ai_error";
      case AG_EVENT_AI_RECOVERED: return "ai_recovered";
      default: return "none";
    }
}

int ag_event_json(char *buffer, size_t size, enum ag_event event,
                  uint64_t monotonic_ms, const struct ag_state *state)
{
  return snprintf(buffer, size,
                  "{\"event\":\"%s\",\"monotonic_ms\":%llu,"
                  "\"present\":%s,\"privacy\":%s,\"paused\":%s}",
                  ag_event_name(event),
                  (unsigned long long)monotonic_ms,
                  state->present ? "true" : "false",
                  state->privacy_enabled ? "true" : "false",
                  state->reminders_paused ? "true" : "false");
}
