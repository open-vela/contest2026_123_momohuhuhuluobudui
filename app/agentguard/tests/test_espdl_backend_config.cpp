#include "dl_define_private.hpp"

static_assert(CONFIG_XTENSA_BOOST == 1,
              "ESP32-S3 must use the official Xtensa backend");
static_assert(CONFIG_TIE728_BOOST == 1,
              "ordinary convolution requires the official TIE728 kernel");
static_assert(CONFIG_AGENTGUARD_ESP_DL_FORCE_C == 0,
              "production inference must not force the diagnostic C backend");
static_assert(CONFIG_AGENTGUARD_ESP_DL_FORCE_C_CONV == 0,
              "ordinary convolution must not use the divergent C path");
static_assert(CONFIG_AGENTGUARD_ESP_DL_FORCE_C_DEPTHWISE == 0,
              "depthwise convolution requires the official TIE728 kernel");
static_assert(CONFIG_AGENTGUARD_FACE_SCORE_THRESHOLD_PERCENT == 50,
              "face detector must use the validated 50 percent threshold");

int main()
{
  return 0;
}
