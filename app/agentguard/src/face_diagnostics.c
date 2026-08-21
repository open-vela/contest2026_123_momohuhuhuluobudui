#include "agentguard/face_diagnostics.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

struct ag_format_buffer
{
  char *buffer;
  size_t size;
  size_t used;
  bool valid;
};

static bool ag_data_fingerprints_match(
  const struct ag_data_fingerprint *left,
  const struct ag_data_fingerprint *right)
{
  return left->valid && right->valid && left->type == right->type &&
         left->elements == right->elements;
}

static void ag_format_append(struct ag_format_buffer *output,
                             const char *format, ...)
{
  va_list arguments;
  int written;

  if (!output->valid)
    {
      return;
    }

  va_start(arguments, format);
  written = vsnprintf(output->buffer + output->used,
                      output->size - output->used, format, arguments);
  va_end(arguments);

  if (written < 0 || (size_t)written >= output->size - output->used)
    {
      output->used = output->size - 1;
      output->valid = false;
      return;
    }

  output->used += (size_t)written;
}

static void ag_format_trace(struct ag_format_buffer *output,
                            const char *name,
                            const struct ag_face_detector_trace *trace,
                            bool change_valid, bool changed)
{
  ag_format_append(output,
                   "%s M=%u A=%u R=%u F=%u N=%u IN=%08lx "
                   "S0=%08lx B0=%08lx S1=%08lx B1=%08lx ICH=%c\n",
                   name, trace->msr_candidates, trace->mnp_attempts,
                   trace->mnp_recorded, trace->final_faces,
                   trace->mnp_accepted, (unsigned long)trace->msr_input.hash,
                   (unsigned long)trace->msr_score0.hash,
                   (unsigned long)trace->msr_box0.hash,
                   (unsigned long)trace->msr_score1.hash,
                   (unsigned long)trace->msr_box1.hash,
                   !change_valid ? '-' : changed ? '1' : '0');
}

bool ag_face_diag_record_mnp(struct ag_face_detector_trace *trace,
                             const struct ag_mnp_diagnostic *diagnostic)
{
  uint8_t index;

  if (trace == NULL || diagnostic == NULL)
    {
      return false;
    }

  if (trace->mnp_attempts < UINT8_MAX)
    {
      trace->mnp_attempts++;
    }

  if (trace->mnp_recorded >= AG_FACE_DIAG_MAX_MNP)
    {
      return false;
    }

  index = trace->mnp_recorded;
  trace->mnp[index] = *diagnostic;
  trace->mnp_recorded++;
  return true;
}

void ag_face_diag_mark_changes(const struct ag_face_diag_snapshot *previous,
                               struct ag_face_diag_snapshot *current)
{
  if (current == NULL)
    {
      return;
    }

  current->raw_change_valid = false;
  current->raw_changed = false;
  current->input_change_valid = false;
  current->input_changed = false;

  if (previous == NULL || !previous->valid || !current->valid)
    {
      return;
    }

  if (previous->raw.valid && current->raw.valid)
    {
      current->raw_change_valid = true;
      current->raw_changed = previous->raw.full_hash != current->raw.full_hash;
    }

  if (ag_data_fingerprints_match(&previous->live.msr_input,
                                 &current->live.msr_input))
    {
      current->input_change_valid = true;
      current->input_changed = previous->live.msr_input.hash !=
                               current->live.msr_input.hash;
    }
}

void ag_face_diag_publish(struct ag_face_diag_store *store,
                          const struct ag_face_diag_snapshot *snapshot)
{
  if (store == NULL || snapshot == NULL)
    {
      return;
    }

  if (pthread_mutex_lock(&store->lock) == 0)
    {
      store->snapshot = *snapshot;
      pthread_mutex_unlock(&store->lock);
    }
}

bool ag_face_diag_get(struct ag_face_diag_store *store,
                      struct ag_face_diag_snapshot *snapshot)
{
  bool valid = false;

  if (store == NULL || snapshot == NULL)
    {
      return false;
    }

  memset(snapshot, 0, sizeof(*snapshot));
  if (pthread_mutex_lock(&store->lock) == 0)
    {
      if (store->snapshot.valid)
        {
          *snapshot = store->snapshot;
          valid = true;
        }

      pthread_mutex_unlock(&store->lock);
    }

  return valid;
}

void ag_face_diag_clear(struct ag_face_diag_store *store)
{
  if (store == NULL)
    {
      return;
    }

  if (pthread_mutex_lock(&store->lock) == 0)
    {
      memset(&store->snapshot, 0, sizeof(store->snapshot));
      pthread_mutex_unlock(&store->lock);
    }
}

bool ag_face_diag_format(char *buffer, size_t size,
                         const struct ag_face_diag_snapshot *snapshot)
{
  struct ag_format_buffer output;
  uint8_t i;

  if (buffer == NULL || size == 0)
    {
      return false;
    }

  buffer[0] = '\0';
  if (snapshot == NULL || !snapshot->valid)
    {
      return false;
    }

  output.buffer = buffer;
  output.size = size;
  output.used = 0;
  output.valid = true;

  ag_format_append(&output, "SEQ=%lu\n",
                   (unsigned long)snapshot->inference_sequence);
  ag_format_append(&output,
                   "RAW=%08lx TOP=%08lx MID=%08lx BOT=%08lx "
                   "MIN=%u MAX=%u NZ=%lu CHG=%c\n",
                   (unsigned long)snapshot->raw.full_hash,
                   (unsigned long)snapshot->raw.top_hash,
                   (unsigned long)snapshot->raw.middle_hash,
                   (unsigned long)snapshot->raw.bottom_hash,
                   snapshot->raw.minimum, snapshot->raw.maximum,
                   (unsigned long)snapshot->raw.nonzero_pixels,
                   !snapshot->raw_change_valid ? '-' :
                     snapshot->raw_changed ? '1' : '0');
  ag_format_append(&output,
                   "REFRAW=%08lx TOP=%08lx MID=%08lx BOT=%08lx\n",
                   (unsigned long)snapshot->reference_raw.full_hash,
                   (unsigned long)snapshot->reference_raw.top_hash,
                   (unsigned long)snapshot->reference_raw.middle_hash,
                   (unsigned long)snapshot->reference_raw.bottom_hash);
  ag_format_trace(&output, "REF", &snapshot->reference, false, false);
  ag_format_trace(&output, "LIVE", &snapshot->live,
                  snapshot->input_change_valid, snapshot->input_changed);

  for (i = 0; i < snapshot->live.mnp_recorded &&
              i < AG_FACE_DIAG_MAX_MNP; i++)
    {
      const struct ag_mnp_diagnostic *mnp = &snapshot->live.mnp[i];

      ag_format_append(&output,
                       "MNP%u C=%u,%u,%u,%u I=%08lx S=%08lx B=%08lx "
                       "L=%08lx P=%u V=%u\n",
                       i, mnp->crop.x, mnp->crop.y, mnp->crop.width,
                       mnp->crop.height, (unsigned long)mnp->input.hash,
                       (unsigned long)mnp->score.hash,
                       (unsigned long)mnp->box.hash,
                       (unsigned long)mnp->landmark.hash,
                       mnp->score_percent, mnp->valid ? 1 : 0);
    }

  return output.valid;
}
