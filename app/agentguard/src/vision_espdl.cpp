/* SPDX-License-Identifier: Apache-2.0 */

#include "agentguard/vision_model.h"
#include "agentguard/espdl_tie_selftest.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <list>
#include <time.h>

#include "dl_image_define.hpp"
#include "human_face_detect.hpp"

namespace
{

constexpr unsigned int kInferenceInterval = 3;
constexpr unsigned int kMaximumFaces = 8;
constexpr float kFaceScoreThreshold =
  CONFIG_AGENTGUARD_FACE_SCORE_THRESHOLD_PERCENT / 100.0f;
constexpr uint32_t kCameraImageCaps =
  dl::image::DL_IMAGE_CAP_RGB_SWAP |
  dl::image::DL_IMAGE_CAP_RGB565_BIG_ENDIAN;

extern "C" const uint8_t _binary_human_face_rgb565be_start[];

HumanFaceDetect *g_detector;
ag_vision_result g_cached_result;
unsigned int g_frame_counter;
ag_vision_model_diagnostics g_diagnostics;

uint64_t monotonic_ms()
{
  struct timespec now;

  if (clock_gettime(CLOCK_MONOTONIC, &now) < 0)
    {
      return 0;
    }

  return static_cast<uint64_t>(now.tv_sec) * 1000u +
         static_cast<uint64_t>(now.tv_nsec) / 1000000u;
}

uint16_t clip_coordinate(int value, uint16_t limit)
{
  if (value < 0)
    {
      return 0;
    }

  if (value >= limit)
    {
      return limit - 1;
    }

  return static_cast<uint16_t>(value);
}

} // namespace

extern "C" void
ag_vision_model_get_diagnostics(
  struct ag_vision_model_diagnostics *diagnostics)
{
  if (diagnostics != nullptr)
    {
      *diagnostics = g_diagnostics;
    }
}

extern "C" int
ag_vision_model_process_rgb565(const uint16_t *pixels, uint16_t width,
                               uint16_t height,
                               struct ag_vision_result *result)
{
  if (pixels == nullptr || result == nullptr || width == 0 || height == 0)
    {
      return -1;
    }

  if (g_detector == nullptr)
    {
      bool tie_ram_pass = false;
      bool tie_flash_pass = false;

      g_diagnostics.tie_selftest_valid =
        ag_espdl_tie_conv_selftest_get_result(&tie_ram_pass,
                                              &tie_flash_pass) ? 1 : 0;
      g_diagnostics.tie_ram_pass = tie_ram_pass ? 1 : 0;
      g_diagnostics.tie_flash_pass = tie_flash_pass ? 1 : 0;
      g_detector = new HumanFaceDetect(HumanFaceDetect::MSRMNP_S8_V1, false);
      if (g_detector == nullptr)
        {
          std::fprintf(stderr, "agentguard: ESP-DL detector allocation failed\n");
          return -1;
        }

      g_detector->set_score_thr(kFaceScoreThreshold, 0);
      g_detector->set_score_thr(kFaceScoreThreshold, 1);
      g_detector->set_image_caps(kCameraImageCaps);

      dl::image::img_t reference_image = {
        .data = const_cast<uint8_t *>(_binary_human_face_rgb565be_start),
        .width = 320,
        .height = 240,
        .pix_type = dl::image::DL_IMAGE_PIX_TYPE_RGB565,
      };
      std::list<dl::detect::result_t> &reference_detections =
        g_detector->run(reference_image);
      int16_t reference_input_min;
      int16_t reference_input_max;
      g_detector->get_last_msr_signal(&reference_input_min,
                                      &reference_input_max,
                                      &g_diagnostics.reference_score_percent);
      g_diagnostics.reference_face_count = static_cast<uint8_t>(
        std::min(reference_detections.size(), static_cast<size_t>(UINT8_MAX)));
    }

  if (g_frame_counter++ % kInferenceInterval != 0)
    {
      *result = g_cached_result;
      return 0;
    }

  dl::image::img_t image = {
    .data = const_cast<uint16_t *>(pixels),
    .width = width,
    .height = height,
    .pix_type = dl::image::DL_IMAGE_PIX_TYPE_RGB565,
  };
  uint64_t inference_started_ms = monotonic_ms();
  std::list<dl::detect::result_t> &detections = g_detector->run(image);
  uint64_t inference_finished_ms = monotonic_ms();
  ag_vision_result next = {};
  int largest_area = 0;

  for (const auto &detection : detections)
    {
      if (detection.box.size() < 4 ||
          next.face_count >= kMaximumFaces)
        {
          continue;
        }

      uint16_t left = clip_coordinate(detection.box[0], width);
      uint16_t top = clip_coordinate(detection.box[1], height);
      uint16_t right = clip_coordinate(detection.box[2], width);
      uint16_t bottom = clip_coordinate(detection.box[3], height);
      if (right <= left || bottom <= top)
        {
          continue;
        }

      next.face_count++;
      int area = static_cast<int>(right - left + 1) *
                 static_cast<int>(bottom - top + 1);
      if (area > largest_area)
        {
          largest_area = area;
          next.primary_face.x = left;
          next.primary_face.y = top;
          next.primary_face.width = right - left + 1;
          next.primary_face.height = bottom - top + 1;
        }
    }

  size_t msr_candidates = g_detector->get_last_msr_candidate_count();
  g_detector->get_last_msr_signal(&g_diagnostics.input_min,
                                  &g_diagnostics.input_max,
                                  &g_diagnostics.score_percent);
  g_diagnostics.msr_candidates = static_cast<uint8_t>(
    std::min(msr_candidates, static_cast<size_t>(UINT8_MAX)));
  g_diagnostics.inference_ms = inference_finished_ms >= inference_started_ms ?
    static_cast<uint32_t>(std::min(
      inference_finished_ms - inference_started_ms,
      static_cast<uint64_t>(UINT32_MAX))) : 0;
  g_diagnostics.valid = 1;
  g_cached_result = next;
  *result = next;
  return 0;
}
