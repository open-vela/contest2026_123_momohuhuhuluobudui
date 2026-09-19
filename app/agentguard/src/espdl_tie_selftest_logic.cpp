/* SPDX-License-Identifier: Apache-2.0 */

#include "agentguard/espdl_tie_selftest.h"

#include <cstdint>

namespace
{

constexpr size_t kTieLaneCount = 16;

int8_t saturate_int8(int32_t value)
{
  if (value > INT8_MAX)
    {
      return INT8_MAX;
    }

  if (value < INT8_MIN)
    {
      return INT8_MIN;
    }

  return static_cast<int8_t>(value);
}

} // namespace

extern "C" int8_t
ag_tie_selftest_scalar_lane(const int8_t input[16],
                            const int8_t filter[256], size_t lane)
{
  if (input == nullptr || filter == nullptr || lane >= kTieLaneCount)
    {
      return 0;
    }

  int32_t accumulator = 0;
  for (size_t channel = 0; channel < kTieLaneCount; ++channel)
    {
      accumulator += static_cast<int32_t>(input[channel]) *
                     static_cast<int32_t>(
                       filter[lane * kTieLaneCount + channel]);
    }

  return saturate_int8(accumulator);
}

extern "C" bool
ag_tie_selftest_all_equal(const int8_t output[16], int8_t expected)
{
  if (output == nullptr)
    {
      return false;
    }

  for (size_t lane = 0; lane < kTieLaneCount; ++lane)
    {
      if (output[lane] != expected)
        {
          return false;
        }
    }

  return true;
}

extern "C" enum ag_tie_selftest_classification
ag_tie_selftest_classify(bool ram_pass, bool flash_pass)
{
  if (ram_pass && !flash_pass)
    {
      return AG_TIE_RAM_PASS_FLASH_FAIL;
    }

  if (!ram_pass && !flash_pass)
    {
      return AG_TIE_RAM_FAIL_FLASH_FAIL;
    }

  if (ram_pass && flash_pass)
    {
      return AG_TIE_RAM_PASS_FLASH_PASS;
    }

  return AG_TIE_RAM_FAIL_FLASH_PASS;
}

extern "C" const char *
ag_tie_selftest_classification_name(
  enum ag_tie_selftest_classification value)
{
  switch (value)
    {
      case AG_TIE_RAM_PASS_FLASH_FAIL:
        return "RAM_PASS_FLASH_FAIL";
      case AG_TIE_RAM_FAIL_FLASH_FAIL:
        return "RAM_FAIL_FLASH_FAIL";
      case AG_TIE_RAM_PASS_FLASH_PASS:
        return "RAM_PASS_FLASH_PASS";
      case AG_TIE_RAM_FAIL_FLASH_PASS:
        return "RAM_FAIL_FLASH_PASS";
    }

  return "UNKNOWN";
}

extern "C" bool ag_tie_selftest_claim(bool *already_ran)
{
  if (already_ran == nullptr || *already_ran)
    {
      return false;
    }

  *already_ran = true;
  return true;
}
