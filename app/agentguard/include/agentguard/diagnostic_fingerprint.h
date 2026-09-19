#ifndef AGENTGUARD_DIAGNOSTIC_FINGERPRINT_H
#define AGENTGUARD_DIAGNOSTIC_FINGERPRINT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

enum ag_fingerprint_type
{
  AG_FINGERPRINT_I8 = 1,
  AG_FINGERPRINT_I16 = 2
};

struct ag_data_fingerprint
{
  uint32_t hash;
  uint32_t elements;
  int16_t minimum;
  int16_t maximum;
  uint8_t type;
  bool valid;
};

struct ag_rgb565_fingerprint
{
  uint32_t full_hash;
  uint32_t top_hash;
  uint32_t middle_hash;
  uint32_t bottom_hash;
  uint32_t nonzero_pixels;
  uint16_t minimum;
  uint16_t maximum;
  bool valid;
};

bool ag_fingerprint_i8(const int8_t *data, size_t elements,
                       struct ag_data_fingerprint *output);
bool ag_fingerprint_i16(const int16_t *data, size_t elements,
                        struct ag_data_fingerprint *output);
bool ag_fingerprint_rgb565(const uint16_t *pixels, uint16_t width,
                           uint16_t height,
                           struct ag_rgb565_fingerprint *output);

#ifdef __cplusplus
}
#endif

#endif
