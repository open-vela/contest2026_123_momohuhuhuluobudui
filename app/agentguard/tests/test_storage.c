/* SPDX-License-Identifier: Apache-2.0 */

#include "agentguard/storage.h"

#include <assert.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#define DAY_MS (24ull * 60ull * 60ull * 1000ull)

static void test_retention_and_stats(void)
{
  const char *path = "/tmp/agentguard-storage-test.jsonl";
  const char *old_event =
    "{\"event\":\"posture_alert\",\"monotonic_ms\":1}";
  const char *new_event =
    "{\"event\":\"break_started\",\"monotonic_ms\":2}";
  struct ag_log_policy policy =
  {
    .retention_ms = 7 * DAY_MS,
    .max_bytes = 4096,
  };
  struct ag_log_stats stats;
  uint64_t now = AG_STORAGE_VALID_UNIX_MS + 20 * DAY_MS;

  unlink(path);
  assert(ag_log_append(path, old_event, now - 8 * DAY_MS, &policy) == 0);
  assert(ag_log_append(path, new_event, now, &policy) == 0);
  assert(ag_log_compact(path, now, &policy) == 0);
  assert(ag_log_read_stats(path, &stats) == 0);
  assert(stats.total_events == 1);
  assert(stats.malformed_lines == 0);
  assert(ag_log_event_count(&stats, AG_EVENT_POSTURE_ALERT) == 0);
  assert(ag_log_event_count(&stats, AG_EVENT_BREAK_STARTED) == 1);
  assert(stats.first_unix_ms == now);
  assert(stats.last_unix_ms == now);
  unlink(path);
}

static void test_size_fallback_without_wall_clock(void)
{
  const char *path = "/tmp/agentguard-storage-size-test.jsonl";
  const char *event =
    "{\"event\":\"sedentary_alert\",\"monotonic_ms\":1}";
  struct ag_log_policy policy =
  {
    .retention_ms = 7 * DAY_MS,
    .max_bytes = 220,
  };
  struct ag_log_stats stats;
  struct stat status;
  int i;

  unlink(path);
  for (i = 0; i < 10; i++)
    {
      assert(ag_log_append(path, event, 0, &policy) == 0);
    }

  assert(stat(path, &status) == 0);
  assert((size_t)status.st_size <= policy.max_bytes);
  assert(ag_log_read_stats(path, &stats) == 0);
  assert(stats.total_events > 0 && stats.total_events < 10);
  assert(ag_log_event_count(&stats, AG_EVENT_SEDENTARY_ALERT) ==
         stats.total_events);
  unlink(path);
}

int main(void)
{
  test_retention_and_stats();
  test_size_fallback_without_wall_clock();
  puts("AgentGuard storage tests: PASS");
  return 0;
}
