/* SPDX-License-Identifier: Apache-2.0 */
#include "agentguard/button_ladder.h"

#include <assert.h>
#include <stdio.h>

int main(void)
{
  struct ag_button_ladder_state state;

  assert(ag_button_ladder_decode(380) == AG_FUNCTION_BUTTON_DOWN);
  assert(ag_button_ladder_decode(820) == AG_FUNCTION_BUTTON_UP);
  assert(ag_button_ladder_decode(1980) == AG_FUNCTION_BUTTON_PLAY);
  assert(ag_button_ladder_decode(2410) == AG_FUNCTION_BUTTON_MENU);
  assert(ag_button_ladder_decode(1331) == AG_FUNCTION_BUTTON_NONE);
  assert(ag_button_ladder_decode(3100) == AG_FUNCTION_BUTTON_NONE);

  ag_button_ladder_init(&state);
  assert(ag_button_ladder_update(&state, 820) == AG_FUNCTION_BUTTON_NONE);
  assert(ag_button_ladder_update(&state, 820) == AG_FUNCTION_BUTTON_NONE);
  assert(ag_button_ladder_update(&state, 1331) == AG_FUNCTION_BUTTON_NONE);
  assert(ag_button_ladder_update(&state, 1331) == AG_FUNCTION_BUTTON_UP);

  puts("AgentGuard button ladder tests: PASS");
  return 0;
}
