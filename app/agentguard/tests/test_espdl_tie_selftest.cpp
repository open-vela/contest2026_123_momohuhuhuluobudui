/* SPDX-License-Identifier: Apache-2.0 */

#include "agentguard/espdl_tie_selftest.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>

static_assert(AG_TIE_SELFTEST_STAGE_IDLE == 0);
static_assert(AG_TIE_SELFTEST_STAGE_RAM == 40);
static_assert(AG_TIE_SELFTEST_STAGE_FLASH == 41);
static_assert(AG_TIE_SELFTEST_STAGE_COMPLETE == 42);

int main()
{
  alignas(16) int8_t input[16];
  alignas(16) int8_t filter[256];
  alignas(16) int8_t output[16];

  std::fill_n(input, 16, int8_t{1});
  std::fill_n(filter, 256, int8_t{1});
  std::fill_n(output, 16, int8_t{16});

  for (size_t lane = 0; lane < 16; ++lane)
    {
      assert(ag_tie_selftest_scalar_lane(input, filter, lane) == 16);
    }

  assert(ag_tie_selftest_all_equal(output, 16));
  output[7] = 15;
  assert(!ag_tie_selftest_all_equal(output, 16));

  assert(ag_tie_selftest_classify(true, false) ==
         AG_TIE_RAM_PASS_FLASH_FAIL);
  assert(ag_tie_selftest_classify(false, false) ==
         AG_TIE_RAM_FAIL_FLASH_FAIL);
  assert(ag_tie_selftest_classify(true, true) ==
         AG_TIE_RAM_PASS_FLASH_PASS);
  assert(ag_tie_selftest_classify(false, true) ==
         AG_TIE_RAM_FAIL_FLASH_PASS);

  assert(std::strcmp(ag_tie_selftest_classification_name(
                       AG_TIE_RAM_PASS_FLASH_FAIL),
                     "RAM_PASS_FLASH_FAIL") == 0);
  assert(std::strcmp(ag_tie_selftest_classification_name(
                       AG_TIE_RAM_FAIL_FLASH_FAIL),
                     "RAM_FAIL_FLASH_FAIL") == 0);
  assert(std::strcmp(ag_tie_selftest_classification_name(
                       AG_TIE_RAM_PASS_FLASH_PASS),
                     "RAM_PASS_FLASH_PASS") == 0);
  assert(std::strcmp(ag_tie_selftest_classification_name(
                       AG_TIE_RAM_FAIL_FLASH_PASS),
                     "RAM_FAIL_FLASH_PASS") == 0);

  bool already_ran = false;
  assert(ag_tie_selftest_claim(&already_ran));
  assert(already_ran);
  assert(!ag_tie_selftest_claim(&already_ran));
  assert(!ag_tie_selftest_claim(nullptr));

  return 0;
}
