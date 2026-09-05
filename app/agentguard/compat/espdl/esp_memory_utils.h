#pragma once

#ifdef __cplusplus
extern "C" {
#endif
bool esp_ptr_external_ram(const void *pointer);
bool esp_ptr_in_drom(const void *pointer);
bool esp_ptr_internal(const void *pointer);
#ifdef __cplusplus
}
#endif
