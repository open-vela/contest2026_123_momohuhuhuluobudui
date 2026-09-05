/* SPDX-License-Identifier: Apache-2.0 */

#include "agentguard/core.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint32_t step(struct ag_state *state, struct ag_config *config,
                     uint64_t time_ms, uint8_t faces, uint8_t posture,
                     enum ag_command command)
{
  struct ag_observation observation =
  {
    .monotonic_ms = time_ms,
    .face_count = faces,
    .posture_score = posture,
    .command = command,
  };

  return ag_step(state, config, &observation);
}

static void test_sedentary_and_lock(void)
{
  struct ag_state state;
  struct ag_config config;

  ag_init(&state);
  ag_default_config(&config);
  config.sedentary_ms = 1000;
  config.reminder_grace_ms = 500;

  assert(step(&state, &config, 100, 1, 0, AG_COMMAND_NONE) &
         AG_EVENT_PRESENCE_STARTED);
  assert(step(&state, &config, 1099, 1, 0, AG_COMMAND_NONE) == 0);
  assert(step(&state, &config, 1100, 1, 0, AG_COMMAND_NONE) &
         AG_EVENT_SEDENTARY_ALERT);
  assert(step(&state, &config, 1599, 1, 0, AG_COMMAND_NONE) == 0);
  assert(step(&state, &config, 1600, 1, 0, AG_COMMAND_NONE) &
         AG_EVENT_LOCK_SCREEN);
}

static void test_ack_prevents_lock(void)
{
  struct ag_state state;
  struct ag_config config;

  ag_init(&state);
  ag_default_config(&config);
  config.sedentary_ms = 100;
  config.reminder_grace_ms = 100;

  step(&state, &config, 1, 1, 0, AG_COMMAND_NONE);
  step(&state, &config, 101, 1, 0, AG_COMMAND_NONE);
  assert(step(&state, &config, 150, 1, 0, AG_COMMAND_ACKNOWLEDGE) &
         AG_EVENT_ACKNOWLEDGED);
  assert(!(step(&state, &config, 300, 1, 0, AG_COMMAND_NONE) &
           AG_EVENT_LOCK_SCREEN));
}

static void test_privacy_fence(void)
{
  struct ag_state state;
  struct ag_config config;
  uint32_t events;

  ag_init(&state);
  ag_default_config(&config);
  events = step(&state, &config, 10, 1, 0, AG_COMMAND_PRIVACY_ON);
  assert(events & AG_EVENT_PRIVACY_ON);
  assert(step(&state, &config, 20, 2, 0, AG_COMMAND_NONE) &
         AG_EVENT_BLUR_SCREEN);
  assert(step(&state, &config, 30, 1, 0, AG_COMMAND_NONE) &
         AG_EVENT_UNBLUR_SCREEN);
}

static void test_posture_and_break(void)
{
  struct ag_state state;
  struct ag_config config;

  ag_init(&state);
  ag_default_config(&config);
  config.posture_hold_ms = 100;
  config.break_confirm_ms = 200;
  step(&state, &config, 10, 1, 80, AG_COMMAND_NONE);
  assert(step(&state, &config, 110, 1, 80, AG_COMMAND_NONE) &
         AG_EVENT_POSTURE_ALERT);
  step(&state, &config, 200, 0, 0, AG_COMMAND_NONE);
  assert(step(&state, &config, 400, 0, 0, AG_COMMAND_NONE) &
         AG_EVENT_BREAK_STARTED);
}

static void test_phrases_and_json(void)
{
  struct ag_state state;
  char output[256];

  assert(ag_parse_command("开启隐私模式") == AG_COMMAND_PRIVACY_ON);
  assert(ag_parse_command("brightness down") ==
         AG_COMMAND_BRIGHTNESS_DOWN);
  assert(ag_parse_command("unknown") == AG_COMMAND_NONE);

  ag_init(&state);
  state.present = true;
  assert(ag_event_json(output, sizeof(output), AG_EVENT_POSTURE_ALERT,
                       123, &state) > 0);
  assert(strstr(output, "posture_alert") != NULL);
}

int main(void)
{
  test_sedentary_and_lock();
  test_ack_prevents_lock();
  test_privacy_fence();
  test_posture_and_break();
  test_phrases_and_json();
  puts("AgentGuard core tests: PASS");
  return 0;
}
