#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "agentguard/face_diagnostics.h"

static void test_tensor_adapter_accepts_only_signed_i8_and_i16(void)
{
  const int8_t i8[4] = {0, 1, 2, -1};
  const int16_t i16[2] = {-2, 258};
  struct ag_data_fingerprint data;

  assert(ag_face_diag_fingerprint_tensor(i8, 4, 1, true, &data));
  assert(data.type == AG_FINGERPRINT_I8);
  assert(data.hash == UINT32_C(0x6fab6075));
  assert(ag_face_diag_fingerprint_tensor(i16, 2, 2, true, &data));
  assert(data.type == AG_FINGERPRINT_I16);
  assert(data.hash == UINT32_C(0xdefc708b));

  memset(&data, 0xa5, sizeof(data));
  assert(!ag_face_diag_fingerprint_tensor(i8, 4, 4, true, &data));
  assert(memcmp(&data, &(struct ag_data_fingerprint){0}, sizeof(data)) == 0);
  memset(&data, 0xa5, sizeof(data));
  assert(!ag_face_diag_fingerprint_tensor(i8, 4, 1, false, &data));
  assert(memcmp(&data, &(struct ag_data_fingerprint){0}, sizeof(data)) == 0);
  assert(!ag_face_diag_fingerprint_tensor(i8, 4, 1, true, NULL));
}

static void test_trace_records_only_first_four_mnp_attempts(void)
{
  struct ag_face_detector_trace trace = {0};
  struct ag_mnp_diagnostic mnp = {0};
  unsigned int i;

  for (i = 0; i < 5; i++)
    {
      mnp.crop.x = (uint16_t)(10 + i);
      assert(ag_face_diag_record_mnp(&trace, &mnp) ==
             (i < AG_FACE_DIAG_MAX_MNP));
    }

  assert(trace.mnp_attempts == 5);
  assert(trace.mnp_recorded == AG_FACE_DIAG_MAX_MNP);
  assert(trace.mnp[0].crop.x == 10);
  assert(trace.mnp[3].crop.x == 13);

  trace.mnp_attempts = UINT8_MAX;
  assert(!ag_face_diag_record_mnp(&trace, &mnp));
  assert(trace.mnp_attempts == UINT8_MAX);
  assert(!ag_face_diag_record_mnp(NULL, &mnp));
  assert(!ag_face_diag_record_mnp(&trace, NULL));
}

static void test_store_copies_complete_valid_snapshots(void)
{
  struct ag_face_diag_store store = AG_FACE_DIAG_STORE_INITIALIZER;
  struct ag_face_diag_snapshot current = {0};
  struct ag_face_diag_snapshot copy = {0};

  assert(!ag_face_diag_get(&store, &copy));
  current.valid = true;
  current.inference_sequence = 12;
  current.raw.valid = true;
  current.raw.full_hash = UINT32_C(0x1585f8f2);
  current.live.mnp[0].crop.x = 17;
  ag_face_diag_publish(&store, &current);

  assert(ag_face_diag_get(&store, &copy));
  assert(copy.inference_sequence == 12);
  assert(copy.live.mnp[0].crop.x == 17);
  current.inference_sequence = 99;
  assert(copy.inference_sequence == 12);

  ag_face_diag_clear(&store);
  assert(!ag_face_diag_get(&store, &current));
  assert(!ag_face_diag_get(NULL, &copy));
  assert(!ag_face_diag_get(&store, NULL));
  ag_face_diag_publish(NULL, &copy);
  ag_face_diag_publish(&store, NULL);
  ag_face_diag_clear(NULL);
}

static void test_change_flags_require_comparable_generations(void)
{
  struct ag_face_diag_snapshot previous = {0};
  struct ag_face_diag_snapshot current = {0};

  current.valid = true;
  current.raw.valid = true;
  current.raw.full_hash = UINT32_C(0x1585f8f2);
  current.live.msr_input.valid = true;
  current.live.msr_input.type = AG_FINGERPRINT_I8;
  current.live.msr_input.elements = 4;
  current.live.msr_input.hash = UINT32_C(0x22222222);

  ag_face_diag_mark_changes(NULL, &current);
  assert(!current.raw_change_valid);
  assert(!current.input_change_valid);

  previous = current;
  current.raw.full_hash++;
  previous.live.msr_input.hash = UINT32_C(0x11111111);
  ag_face_diag_mark_changes(&previous, &current);
  assert(current.raw_change_valid && current.raw_changed);
  assert(current.input_change_valid && current.input_changed);

  previous.live.msr_input.type = AG_FINGERPRINT_I16;
  ag_face_diag_mark_changes(&previous, &current);
  assert(current.raw_change_valid);
  assert(!current.input_change_valid);
  assert(!current.input_changed);

  ag_face_diag_mark_changes(&previous, NULL);
}

static void test_snapshot_assembly_compares_only_same_stage_hashes(void)
{
  struct ag_rgb565_fingerprint reference_raw = {0};
  struct ag_rgb565_fingerprint raw = {0};
  struct ag_face_detector_trace reference = {0};
  struct ag_face_detector_trace live = {0};
  struct ag_face_diag_snapshot first;
  struct ag_face_diag_snapshot second;

  reference_raw.valid = true;
  reference_raw.full_hash = UINT32_C(0x01020304);
  raw.valid = true;
  raw.full_hash = UINT32_C(0x11111111);
  reference.valid = true;
  reference.msr_candidates = 2;
  live.valid = true;
  live.final_faces = 1;
  live.msr_input.valid = true;
  live.msr_input.type = AG_FINGERPRINT_I8;
  live.msr_input.elements = 4;
  live.msr_input.hash = UINT32_C(0x22222222);

  ag_face_diag_prepare_snapshot(NULL, 1, &reference_raw, &raw,
                                &reference, &live, &first);
  assert(first.valid);
  assert(first.inference_sequence == 1);
  assert(first.reference_raw.full_hash == UINT32_C(0x01020304));
  assert(first.reference.msr_candidates == 2);
  assert(first.live.final_faces == 1);
  assert(!first.raw_change_valid);
  assert(!first.input_change_valid);

  raw.full_hash = UINT32_C(0x11111112);
  raw.top_hash = UINT32_C(0xaaaaaaaa);
  live.msr_input.hash = UINT32_C(0x22222223);
  live.msr_score0.hash = UINT32_C(0xbbbbbbbb);
  ag_face_diag_prepare_snapshot(&first, 2, &reference_raw, &raw,
                                &reference, &live, &second);
  assert(second.valid);
  assert(second.inference_sequence == 2);
  assert(second.raw_change_valid && second.raw_changed);
  assert(second.input_change_valid && second.input_changed);
  assert(second.raw.top_hash == UINT32_C(0xaaaaaaaa));
  assert(second.live.msr_score0.hash == UINT32_C(0xbbbbbbbb));

  memset(&second, 0xa5, sizeof(second));
  ag_face_diag_prepare_snapshot(&first, 3, &reference_raw, NULL,
                                &reference, &live, &second);
  assert(memcmp(&second, &(struct ag_face_diag_snapshot){0},
                sizeof(second)) == 0);
  ag_face_diag_prepare_snapshot(&first, 3, &reference_raw, &raw,
                                &reference, &live, NULL);
}

static void test_format_reports_fixed_order_snapshot_without_overflow(void)
{
  struct ag_face_diag_snapshot snapshot = {0};
  char text[768];
  unsigned char small[8];

  snapshot.valid = true;
  snapshot.inference_sequence = 12;
  snapshot.raw.valid = true;
  snapshot.raw.full_hash = UINT32_C(0x1585f8f3);
  snapshot.raw_change_valid = true;
  snapshot.raw_changed = true;
  snapshot.live.valid = true;
  snapshot.live.msr_input.valid = true;
  snapshot.live.msr_input.type = AG_FINGERPRINT_I8;
  snapshot.live.msr_input.elements = 4;
  snapshot.live.msr_input.hash = UINT32_C(0x22222222);
  snapshot.input_change_valid = true;
  snapshot.input_changed = true;
  snapshot.live.msr_candidates = 1;
  snapshot.live.mnp_attempts = 5;
  snapshot.live.mnp_recorded = 4;
  snapshot.live.final_faces = 0;

  assert(ag_face_diag_format(text, sizeof(text), &snapshot));
  assert(strstr(text, "SEQ=12") != NULL);
  assert(strstr(text, "RAW=1585f8f3") != NULL);
  assert(strstr(text, "M=1 A=5 R=4 F=0") != NULL);

  memset(small, 0xa5, sizeof(small));
  assert(!ag_face_diag_format((char *)small, 4, &snapshot));
  assert(small[3] == '\0');
  assert(small[4] == 0xa5);
  assert(!ag_face_diag_format(NULL, sizeof(text), &snapshot));
  assert(!ag_face_diag_format(text, sizeof(text), NULL));

  snapshot.valid = false;
  assert(!ag_face_diag_format(text, sizeof(text), &snapshot));
}

int main(void)
{
  test_tensor_adapter_accepts_only_signed_i8_and_i16();
  test_trace_records_only_first_four_mnp_attempts();
  test_store_copies_complete_valid_snapshots();
  test_change_flags_require_comparable_generations();
  test_snapshot_assembly_compares_only_same_stage_hashes();
  test_format_reports_fixed_order_snapshot_without_overflow();
  puts("AgentGuard face diagnostics tests: PASS");
  return 0;
}
