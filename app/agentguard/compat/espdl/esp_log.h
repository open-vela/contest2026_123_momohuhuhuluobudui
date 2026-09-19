#pragma once

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif
uint32_t esp_log_timestamp(void);
void esp_log_write(int level, const char *tag, const char *format, ...);
#ifdef __cplusplus
}
#endif

#define ESP_LOGE(tag, fmt, ...) \
  fprintf(stderr, "E %s: " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGW(tag, fmt, ...) \
  printf("W %s: " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGI(tag, fmt, ...) \
  printf("I %s: " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGD(tag, fmt, ...) do { } while (0)
#define ESP_ERROR_CHECK(value)                                                \
  do                                                                          \
    {                                                                         \
      int ag_esp_error_check_result = (value);                                \
      if (ag_esp_error_check_result != 0)                                     \
        {                                                                     \
          fprintf(stderr, "ESP_ERROR_CHECK failed: %d at %s:%d\n",           \
                  ag_esp_error_check_result, __FILE__, __LINE__);             \
          abort();                                                            \
        }                                                                     \
    }                                                                         \
  while (0)
