#pragma once

#ifdef __cplusplus
extern "C" {
#endif
int dsps_dotprod_f32(const float *input_a, const float *input_b,
                     float *output, int length);
#ifdef __cplusplus
}
#endif
