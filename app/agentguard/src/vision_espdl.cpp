/* SPDX-License-Identifier: Apache-2.0 */

#include "agentguard/vision_model.h"
#include "agentguard/espdl_camera_adapter.h"
#include "agentguard/espdl_tie_selftest.h"

#include <algorithm>
#include <cstdlib>
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

extern "C" const uint8_t _binary_human_face_rgb565be_start[];

HumanFaceDetect *g_detector;
ag_vision_result g_cached_result;
unsigned int g_frame_counter;
ag_vision_model_diagnostics g_diagnostics;
ag_face_diag_store g_face_diag_store = AG_FACE_DIAG_STORE_INITIALIZER;
ag_rgb565_fingerprint g_reference_raw;
ag_face_detector_trace g_reference_trace;
ag_face_diag_snapshot g_previous_snapshot;
uint32_t g_face_diag_sequence;
uint16_t *g_model_rgb565be;
size_t g_model_rgb565be_capacity;

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

uint8_t detection_count(const std::list<dl::detect::result_t> &detections)
{
  return static_cast<uint8_t>(
    std::min(detections.size(), static_cast<size_t>(UINT8_MAX)));
}

uint8_t maximum_score_percent(
  const std::list<dl::detect::result_t> &detections)
{
  float maximum = 0.0f;

  for (const auto &detection : detections)
    {
      maximum = std::max(maximum, detection.score);
    }

  return static_cast<uint8_t>(std::min(maximum * 100.0f, 99.0f));
}

void summarize_detections(
  const std::list<dl::detect::result_t> &detections,
  struct ag_face_detector_trace *trace)
{
  std::memset(trace, 0, sizeof(*trace));
  trace->mnp_accepted = detection_count(detections);
  trace->final_faces = trace->mnp_accepted;
  trace->valid = true;
}

void fingerprint_tensor(dl::TensorBase *tensor,
                        struct ag_data_fingerprint *output)
{
  if (tensor == nullptr || output == nullptr)
    {
      return;
    }

  if (tensor->dtype == dl::DATA_TYPE_INT8)
    {
      ag_face_diag_fingerprint_tensor(
        tensor->get_element_ptr<int8_t>(),
        static_cast<uint32_t>(tensor->get_size()), sizeof(int8_t), true,
        output);
    }
  else if (tensor->dtype == dl::DATA_TYPE_INT16)
    {
      ag_face_diag_fingerprint_tensor(
        tensor->get_element_ptr<int16_t>(),
        static_cast<uint32_t>(tensor->get_size()), sizeof(int16_t), true,
        output);
    }
}

void capture_msr_outputs(HumanFaceDetect *detector,
                         struct ag_face_detector_trace *trace)
{
  dl::Model *model = detector->get_raw_model(0);

  fingerprint_tensor(model->get_output("score0"), &trace->msr_score0);
  fingerprint_tensor(model->get_output("box0"), &trace->msr_box0);
  fingerprint_tensor(model->get_output("score1"), &trace->msr_score1);
  fingerprint_tensor(model->get_output("box1"), &trace->msr_box1);
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

extern "C" bool
ag_vision_model_get_face_diagnostics(
  struct ag_face_diag_snapshot *diagnostics)
{
  return ag_face_diag_get(&g_face_diag_store, diagnostics);
}

extern "C" void ag_vision_model_reset_face_diagnostics(void)
{
  ag_face_diag_clear(&g_face_diag_store);
  std::memset(&g_previous_snapshot, 0, sizeof(g_previous_snapshot));
  g_face_diag_sequence = 0;
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
      g_diagnostics.pixel_mode_scores[0] = 3;

      dl::image::img_t reference_image = {
        .data = const_cast<uint8_t *>(_binary_human_face_rgb565be_start),
        .width = 320,
        .height = 240,
        .pix_type = dl::image::DL_IMAGE_PIX_TYPE_RGB565,
      };
      ag_fingerprint_rgb565(
        reinterpret_cast<const uint16_t *>(
          _binary_human_face_rgb565be_start),
        320, 240, &g_reference_raw);
      std::list<dl::detect::result_t> &reference_detections =
        g_detector->run(reference_image);
      summarize_detections(reference_detections, &g_reference_trace);
      capture_msr_outputs(g_detector, &g_reference_trace);
      g_diagnostics.reference_score_percent =
        maximum_score_percent(reference_detections);
      g_diagnostics.reference_face_count = detection_count(reference_detections);
    }

  if (g_frame_counter++ % kInferenceInterval != 0)
    {
      *result = g_cached_result;
      return 0;
    }

  size_t pixel_count = static_cast<size_t>(width) * height;
  if (pixel_count > SIZE_MAX / sizeof(*g_model_rgb565be))
    {
      return -1;
    }

  if (g_model_rgb565be_capacity < pixel_count)
    {
      void *resized = std::realloc(g_model_rgb565be,
                                   pixel_count * sizeof(uint16_t));
      if (resized == nullptr)
        {
          return -1;
        }

      g_model_rgb565be = static_cast<uint16_t *>(resized);
      g_model_rgb565be_capacity = pixel_count;
    }

  /* NuttX exposes the OV2640 frame as little-endian RGB565 words, while the
   * official ESP32-S3 detector preprocessors consume big-endian RGB565.
   * Keep the camera/display buffer untouched and adapt only the model copy. */

  if (!ag_espdl_camera_rgb565be_copy(g_model_rgb565be, pixels,
                                     pixel_count))
    {
      return -1;
    }

  dl::image::img_t image = {
    .data = g_model_rgb565be,
    .width = width,
    .height = height,
    .pix_type = dl::image::DL_IMAGE_PIX_TYPE_RGB565,
  };
  ag_rgb565_fingerprint raw_fingerprint = {};
  ag_face_detector_trace live_trace = {};
  ag_face_diag_snapshot snapshot = {};

  ag_fingerprint_rgb565(pixels, width, height, &raw_fingerprint);
  uint64_t inference_started_ms = monotonic_ms();
  std::list<dl::detect::result_t> &detections = g_detector->run(image);
  uint64_t inference_finished_ms = monotonic_ms();
  summarize_detections(detections, &live_trace);
  capture_msr_outputs(g_detector, &live_trace);
  g_face_diag_sequence++;
  ag_face_diag_prepare_snapshot(
    g_previous_snapshot.valid ? &g_previous_snapshot : nullptr,
    g_face_diag_sequence, &g_reference_raw, &raw_fingerprint,
    &g_reference_trace, &live_trace, &snapshot);
  ag_face_diag_publish(&g_face_diag_store, &snapshot);
  g_previous_snapshot = snapshot;
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

      ag_face_box face = {
        .x = left,
        .y = top,
        .width = static_cast<uint16_t>(right - left + 1),
        .height = static_cast<uint16_t>(bottom - top + 1),
      };
      next.face_boxes[next.face_box_count++] = face;
      next.face_count++;
      int area = static_cast<int>(right - left + 1) *
                 static_cast<int>(bottom - top + 1);
      if (area > largest_area)
        {
          largest_area = area;
          next.primary_face = face;
        }
    }

  g_diagnostics.input_min = 0;
  g_diagnostics.input_max = 0;
  g_diagnostics.score_percent = maximum_score_percent(detections);
  g_diagnostics.msr_candidates = 0;
  g_diagnostics.inference_ms = inference_finished_ms >= inference_started_ms ?
    static_cast<uint32_t>(std::min(
      inference_finished_ms - inference_started_ms,
      static_cast<uint64_t>(UINT32_MAX))) : 0;
  g_diagnostics.valid = 1;
  g_cached_result = next;
  *result = next;
  return 0;
}
