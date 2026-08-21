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
  test_format_reports_fixed_order_snapshot_without_overflow();
  puts("AgentGuard face diagnostics tests: PASS");
  return 0;
}
