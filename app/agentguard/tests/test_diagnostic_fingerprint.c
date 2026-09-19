#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "agentguard/diagnostic_fingerprint.h"

static void test_signed_tensor_fingerprints_use_stable_byte_order(void)
{
  const int8_t i8_values[4] = {0, 1, 2, -1};
  const int16_t i16_values[2] = {-2, 258};
  struct ag_data_fingerprint data;

  assert(ag_fingerprint_i8(i8_values, 4, &data));
  assert(data.valid);
  assert(data.elements == 4);
  assert(data.type == AG_FINGERPRINT_I8);
  assert(data.minimum == -1);
  assert(data.maximum == 2);
  assert(data.hash == UINT32_C(0x6fab6075));

  assert(ag_fingerprint_i16(i16_values, 2, &data));
  assert(data.valid);
  assert(data.elements == 2);
  assert(data.type == AG_FINGERPRINT_I16);
  assert(data.minimum == -2);
  assert(data.maximum == 258);
  assert(data.hash == UINT32_C(0xdefc708b));
}

static void test_rgb565_fingerprint_covers_three_row_regions(void)
{
  const uint16_t rgb[6] = {1, 2, 3, 4, 5, 6};
  struct ag_rgb565_fingerprint image;

  assert(ag_fingerprint_rgb565(rgb, 2, 3, &image));
  assert(image.valid);
  assert(image.full_hash == UINT32_C(0x1585f8f2));
  assert(image.top_hash == UINT32_C(0x476eaad6));
  assert(image.middle_hash == UINT32_C(0x03b84a82));
  assert(image.bottom_hash == UINT32_C(0x9ec79836));
  assert(image.minimum == 1);
  assert(image.maximum == 6);
  assert(image.nonzero_pixels == 6);
}

static void test_invalid_inputs_fail_and_clear_output(void)
{
  const int8_t i8_values[1] = {1};
  const uint16_t rgb[1] = {1};
  struct ag_data_fingerprint data;
  struct ag_rgb565_fingerprint image;

  memset(&data, 0xa5, sizeof(data));
  assert(!ag_fingerprint_i8(NULL, 1, &data));
  assert(memcmp(&data, &(struct ag_data_fingerprint){0}, sizeof(data)) == 0);

  memset(&data, 0xa5, sizeof(data));
  assert(!ag_fingerprint_i8(i8_values, 0, &data));
  assert(memcmp(&data, &(struct ag_data_fingerprint){0}, sizeof(data)) == 0);
  assert(!ag_fingerprint_i8(i8_values, 1, NULL));

  memset(&image, 0xa5, sizeof(image));
  assert(!ag_fingerprint_rgb565(rgb, 0, 1, &image));
  assert(memcmp(&image, &(struct ag_rgb565_fingerprint){0},
                sizeof(image)) == 0);

  memset(&image, 0xa5, sizeof(image));
  assert(!ag_fingerprint_rgb565(NULL, 1, 1, &image));
  assert(memcmp(&image, &(struct ag_rgb565_fingerprint){0},
                sizeof(image)) == 0);

  assert(!ag_fingerprint_rgb565(rgb, 1, 0, NULL));
}

int main(void)
{
  test_signed_tensor_fingerprints_use_stable_byte_order();
  test_rgb565_fingerprint_covers_three_row_regions();
  test_invalid_inputs_fail_and_clear_output();
  puts("AgentGuard diagnostic fingerprint tests: PASS");
  return 0;
}
