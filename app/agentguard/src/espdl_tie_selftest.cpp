/* SPDX-License-Identifier: Apache-2.0 */

#include "agentguard/espdl_tie_selftest.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>

#include "dl_base.hpp"
#include "dl_base_tie728.h"

namespace
{

constexpr size_t kTieLaneCount = 16;
constexpr size_t kTieFilterElementCount =
  kTieLaneCount * kTieLaneCount;

struct alignas(16) TieFilter
{
  int8_t values[kTieFilterElementCount];
};

constexpr TieFilter make_ones_filter()
{
  TieFilter filter = {};
  for (size_t i = 0; i < kTieFilterElementCount; ++i)
    {
      filter.values[i] = 1;
    }

  return filter;
}

alignas(16) int8_t kInput[kTieLaneCount];
alignas(16) TieFilter kRamFilter;
alignas(16) constexpr TieFilter kFlashFilter = make_ones_filter();
alignas(16) int8_t kRamOutput[kTieLaneCount];
alignas(16) int8_t kFlashOutput[kTieLaneCount];
bool g_selftest_already_ran;

static_assert(sizeof(void *) == 4);
static_assert(offsetof(dl::base::ArgsType<int8_t>, input_element) == 0);
static_assert(offsetof(dl::base::ArgsType<int8_t>, output_element) == 24);
static_assert(offsetof(dl::base::ArgsType<int8_t>, filter_element) == 48);
static_assert(offsetof(dl::base::ArgsType<int8_t>, mac_shift) == 64);
static_assert(offsetof(dl::base::ArgsType<int8_t>, bias_element) == 68);
static_assert(offsetof(dl::base::ArgsType<int8_t>, activation_type) == 72);
static_assert(offsetof(dl::base::ArgsType<int8_t>, n_div_x) == 96);
static_assert(offsetof(dl::base::ArgsType<int8_t>, c_div_x_1) == 100);
static_assert(offsetof(dl::base::ArgsType<int8_t>,
                       tie_filter_channel_factor) == 104);

dl::base::ArgsType<int8_t>
make_args(int8_t *output, const int8_t *filter)
{
  dl::base::ArgsType<int8_t> args = {};

  args.input_element = kInput;
  args.input_channel = kTieLaneCount;
  args.output_element = output;
  args.output_channel = kTieLaneCount;
  args.filter_element = filter;
  args.filter_height = 1;
  args.filter_width = 1;
  args.filter_y_offset = kTieLaneCount;
  args.filter_n_offset = kTieLaneCount;
  args.mac_shift = 0;
  args.bias_element = nullptr;
  args.activation_type = dl::Linear;
  args.n_div_x = 1;
  args.c_div_x_1 = 0;
  return args;
}

void print_vector(const int8_t output[kTieLaneCount])
{
  std::fprintf(stderr, "[");
  for (size_t lane = 0; lane < kTieLaneCount; ++lane)
    {
      std::fprintf(stderr, "%s%d", lane == 0 ? "" : ",",
                   static_cast<int>(output[lane]));
    }

  std::fprintf(stderr, "]");
}

} // namespace

extern "C" void ag_espdl_tie_conv_selftest_run_once(void)
{
  if (!ag_tie_selftest_claim(&g_selftest_already_ran))
    {
      return;
    }

  std::fill_n(kInput, kTieLaneCount, int8_t{1});
  std::fill_n(kRamFilter.values, kTieFilterElementCount, int8_t{1});
  std::fill_n(kRamOutput, kTieLaneCount, INT8_MIN);
  std::fill_n(kFlashOutput, kTieLaneCount, INT8_MIN);

  dl::base::ArgsType<int8_t> ram_args =
    make_args(kRamOutput, kRamFilter.values);
  dl::base::ArgsType<int8_t> flash_args =
    make_args(kFlashOutput, kFlashFilter.values);

  dl_tie728_s8_conv2d_11cn(kRamOutput, kInput, &ram_args);
  dl_tie728_s8_conv2d_11cn(kFlashOutput, kInput, &flash_args);

  int8_t expected = ag_tie_selftest_scalar_lane(
    kInput, kRamFilter.values, 0);
  bool ram_pass = ag_tie_selftest_all_equal(kRamOutput, expected);
  bool flash_pass = ag_tie_selftest_all_equal(kFlashOutput, expected);
  enum ag_tie_selftest_classification classification =
    ag_tie_selftest_classify(ram_pass, flash_pass);

  std::fprintf(stderr,
               "agentguard: TIE_SELFTEST in=%p ram_filter=%p "
               "flash_filter=%p args=%p ram_out=%p flash_out=%p expected=%d "
               "ram=",
               static_cast<void *>(kInput),
               static_cast<void *>(kRamFilter.values),
               static_cast<const void *>(kFlashFilter.values),
               static_cast<void *>(&ram_args),
               static_cast<void *>(kRamOutput),
               static_cast<void *>(kFlashOutput),
               static_cast<int>(expected));
  print_vector(kRamOutput);
  std::fprintf(stderr, " flash=");
  print_vector(kFlashOutput);
  std::fprintf(stderr, " result=%s\n",
               ag_tie_selftest_classification_name(classification));
}
