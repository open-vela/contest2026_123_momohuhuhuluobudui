/* SPDX-License-Identifier: Apache-2.0 */

#ifndef AGENTGUARD_STORAGE_H
#define AGENTGUARD_STORAGE_H

#include "agentguard/core.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AG_STORAGE_VALID_UNIX_MS 1577836800000ull
#define AG_STORAGE_EVENT_SLOTS 14

struct ag_log_policy
{
  uint64_t retention_ms;
  size_t max_bytes;
};

struct ag_log_stats
{
  uint32_t event_counts[AG_STORAGE_EVENT_SLOTS];
  uint32_t total_events;
  uint32_t malformed_lines;
  uint64_t first_unix_ms;
  uint64_t last_unix_ms;
};

int ag_log_append(const char *path, const char *event_json,
                  uint64_t unix_ms, const struct ag_log_policy *policy);
int ag_log_compact(const char *path, uint64_t now_unix_ms,
                   const struct ag_log_policy *policy);
int ag_log_read_stats(const char *path, struct ag_log_stats *stats);
uint32_t ag_log_event_count(const struct ag_log_stats *stats,
                            enum ag_event event);

#ifdef __cplusplus
}
#endif

#endif
