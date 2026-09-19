/* SPDX-License-Identifier: Apache-2.0 */

#ifndef AGENTGUARD_ESPDL_TIE_SELFTEST_H
#define AGENTGUARD_ESPDL_TIE_SELFTEST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

enum ag_tie_selftest_classification
{
  AG_TIE_RAM_PASS_FLASH_FAIL,
  AG_TIE_RAM_FAIL_FLASH_FAIL,
  AG_TIE_RAM_PASS_FLASH_PASS,
  AG_TIE_RAM_FAIL_FLASH_PASS
};

enum ag_tie_selftest_stage
{
  AG_TIE_SELFTEST_STAGE_IDLE = 0,
  AG_TIE_SELFTEST_STAGE_RAM = 40,
  AG_TIE_SELFTEST_STAGE_FLASH = 41,
  AG_TIE_SELFTEST_STAGE_COMPLETE = 42
};

int8_t ag_tie_selftest_scalar_lane(const int8_t input[16],
                                   const int8_t filter[256], size_t lane);
bool ag_tie_selftest_all_equal(const int8_t output[16], int8_t expected);
enum ag_tie_selftest_classification
ag_tie_selftest_classify(bool ram_pass, bool flash_pass);
const char *ag_tie_selftest_classification_name(
  enum ag_tie_selftest_classification value);
bool ag_tie_selftest_claim(bool *already_ran);
void ag_espdl_tie_conv_selftest_run_once(void);
uint8_t ag_espdl_tie_conv_selftest_get_stage(void);
bool ag_espdl_tie_conv_selftest_get_result(bool *ram_pass,
                                           bool *flash_pass);

#ifdef __cplusplus
}
#endif

#endif /* AGENTGUARD_ESPDL_TIE_SELFTEST_H */
