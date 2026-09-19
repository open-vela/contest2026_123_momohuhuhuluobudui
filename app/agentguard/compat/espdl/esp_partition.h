#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#define ESP_PARTITION_TYPE_DATA 1
#define ESP_PARTITION_SUBTYPE_ANY 0xff
#define ESP_PARTITION_MMAP_DATA 0

typedef struct
{
  size_t size;
  const char *label;
} esp_partition_t;
typedef uint32_t esp_partition_mmap_handle_t;

static inline const esp_partition_t *
esp_partition_find_first(int type, int subtype, const char *label)
{
  (void)type;
  (void)subtype;
  (void)label;
  return NULL;
}

static inline esp_err_t
esp_partition_mmap(const esp_partition_t *partition, size_t offset,
                   size_t size, int memory, const void **pointer,
                   esp_partition_mmap_handle_t *handle)
{
  (void)partition;
  (void)offset;
  (void)size;
  (void)memory;
  (void)pointer;
  (void)handle;
  return ESP_ERR_NOT_SUPPORTED;
}

static inline void esp_partition_munmap(esp_partition_mmap_handle_t handle)
{
  (void)handle;
}
