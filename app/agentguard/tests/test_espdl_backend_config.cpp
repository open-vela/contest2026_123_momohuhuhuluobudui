#include "dl_define_private.hpp"

static_assert(CONFIG_XTENSA_BOOST == 1,
              "the mixed diagnostic backend must retain Xtensa support");
static_assert(CONFIG_TIE728_BOOST == 1,
              "the mixed diagnostic backend must retain TIE728 support");
static_assert(CONFIG_AGENTGUARD_ESP_DL_FORCE_C_CONV == 1,
              "ordinary convolution must use the numerically correct C path");
static_assert(CONFIG_AGENTGUARD_ESP_DL_FORCE_C_DEPTHWISE == 1,
              "depthwise convolution must use the numerically correct C path");
static_assert(CONFIG_AGENTGUARD_FACE_SCORE_THRESHOLD_PERCENT == 50,
              "face detector must use the validated 50 percent threshold");

int main()
{
  return 0;
}
