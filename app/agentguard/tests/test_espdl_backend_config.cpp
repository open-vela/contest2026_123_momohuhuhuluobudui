#include "dl_define_private.hpp"

static_assert(CONFIG_XTENSA_BOOST == 1,
              "the mixed diagnostic backend must retain Xtensa support");
static_assert(CONFIG_TIE728_BOOST == 1,
              "the mixed diagnostic backend must retain TIE728 support");
static_assert(CONFIG_AGENTGUARD_ESP_DL_FORCE_C_CONV == 0,
              "ordinary convolution must retain TIE728 acceleration");
static_assert(CONFIG_AGENTGUARD_ESP_DL_FORCE_C_DEPTHWISE == 0,
              "depthwise convolution must retain TIE728 acceleration");

int main()
{
  return 0;
}
