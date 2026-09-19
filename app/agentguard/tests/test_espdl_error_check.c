/* SPDX-License-Identifier: Apache-2.0 */

#include "esp_log.h"

#include <stdio.h>

static int g_evaluations;

int evaluate_success(void)
{
  g_evaluations++;
  return 0;
}

int main(void)
{
  ESP_ERROR_CHECK(evaluate_success());
  if (g_evaluations != 1)
    {
      fprintf(stderr, "ESP_ERROR_CHECK skipped its expression\n");
      return 1;
    }

  puts("AgentGuard ESP-DL error-check tests: PASS");
  return 0;
}
