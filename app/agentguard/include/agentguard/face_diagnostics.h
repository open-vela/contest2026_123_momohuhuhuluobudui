#ifndef AGENTGUARD_FACE_DIAGNOSTICS_H
#define AGENTGUARD_FACE_DIAGNOSTICS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <pthread.h>

#include "agentguard/diagnostic_fingerprint.h"
#include "agentguard/vision.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define AG_FACE_DIAG_MAX_MNP 4

struct ag_face_candidate_diagnostic
{
  int32_t left;
  int32_t top;
  int32_t right;
  int32_t bottom;
  uint16_t score_permille;
  bool valid;
};

struct ag_resize_scale_diagnostic
{
  int32_t x_millionths;
  int32_t y_millionths;
  int32_t inv_x_millionths;
  int32_t inv_y_millionths;
  bool valid;
};

struct ag_mnp_diagnostic
{
  struct ag_face_box crop;
  struct ag_data_fingerprint input;
  struct ag_data_fingerprint score;
  struct ag_data_fingerprint box;
  struct ag_data_fingerprint landmark;
  uint8_t score_percent;
  bool valid;
};

struct ag_face_detector_trace
{
  struct ag_data_fingerprint msr_input;
  struct ag_data_fingerprint msr_score0;
  struct ag_data_fingerprint msr_box0;
  struct ag_data_fingerprint msr_score1;
  struct ag_data_fingerprint msr_box1;
  struct ag_resize_scale_diagnostic msr_resize;
  struct ag_face_box largest_msr;
  struct ag_face_candidate_diagnostic msr_candidate_before_clip;
  struct ag_face_candidate_diagnostic msr_candidate_after_clip;
  struct ag_mnp_diagnostic mnp[AG_FACE_DIAG_MAX_MNP];
  uint8_t msr_candidates;
  uint8_t mnp_attempts;
  uint8_t mnp_recorded;
  uint8_t mnp_accepted;
  uint8_t final_faces;
  bool valid;
};

struct ag_face_diag_snapshot
{
  uint32_t inference_sequence;
  struct ag_rgb565_fingerprint reference_raw;
  struct ag_rgb565_fingerprint raw;
  struct ag_face_detector_trace reference;
  struct ag_face_detector_trace live;
  bool raw_change_valid;
  bool raw_changed;
  bool input_change_valid;
  bool input_changed;
  bool valid;
};

struct ag_face_diag_store
{
  pthread_mutex_t lock;
  struct ag_face_diag_snapshot snapshot;
};

#define AG_FACE_DIAG_STORE_INITIALIZER \
  { PTHREAD_MUTEX_INITIALIZER, {0} }

bool ag_face_diag_record_mnp(struct ag_face_detector_trace *trace,
                             const struct ag_mnp_diagnostic *diagnostic);
bool ag_face_diag_fingerprint_tensor(
  const void *data, uint32_t elements, uint8_t element_bytes,
  bool element_signed, struct ag_data_fingerprint *output);
void ag_face_diag_mark_changes(const struct ag_face_diag_snapshot *previous,
                               struct ag_face_diag_snapshot *current);
void ag_face_diag_publish(struct ag_face_diag_store *store,
                          const struct ag_face_diag_snapshot *snapshot);
bool ag_face_diag_get(struct ag_face_diag_store *store,
                      struct ag_face_diag_snapshot *snapshot);
void ag_face_diag_clear(struct ag_face_diag_store *store);
void ag_face_diag_prepare_snapshot(
  const struct ag_face_diag_snapshot *previous, uint32_t sequence,
  const struct ag_rgb565_fingerprint *reference_raw,
  const struct ag_rgb565_fingerprint *raw,
  const struct ag_face_detector_trace *reference,
  const struct ag_face_detector_trace *live,
  struct ag_face_diag_snapshot *output);
bool ag_face_diag_format(char *buffer, size_t size,
                         const struct ag_face_diag_snapshot *snapshot);

#ifdef __cplusplus
}
#endif

#endif
