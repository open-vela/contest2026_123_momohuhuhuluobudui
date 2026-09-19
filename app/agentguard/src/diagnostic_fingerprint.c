#include "agentguard/diagnostic_fingerprint.h"

#include <limits.h>
#include <string.h>

#define AG_FNV1A_OFFSET UINT32_C(2166136261)
#define AG_FNV1A_PRIME UINT32_C(16777619)

static uint32_t ag_fnv1a_byte(uint32_t hash, uint8_t value)
{
  return (hash ^ value) * AG_FNV1A_PRIME;
}

static uint32_t ag_fnv1a_u16(uint32_t hash, uint16_t value)
{
  hash = ag_fnv1a_byte(hash, (uint8_t)(value & UINT16_C(0x00ff)));
  return ag_fnv1a_byte(hash, (uint8_t)(value >> 8));
}

bool ag_fingerprint_i8(const int8_t *data, size_t elements,
                       struct ag_data_fingerprint *output)
{
  uint32_t hash = AG_FNV1A_OFFSET;
  int8_t minimum;
  int8_t maximum;
  size_t i;

  if (output == NULL)
    {
      return false;
    }

  memset(output, 0, sizeof(*output));
  if (data == NULL || elements == 0 || elements > UINT32_MAX)
    {
      return false;
    }

  minimum = data[0];
  maximum = data[0];
  for (i = 0; i < elements; i++)
    {
      if (data[i] < minimum)
        {
          minimum = data[i];
        }

      if (data[i] > maximum)
        {
          maximum = data[i];
        }

      hash = ag_fnv1a_byte(hash, (uint8_t)data[i]);
    }

  output->hash = hash;
  output->elements = (uint32_t)elements;
  output->minimum = minimum;
  output->maximum = maximum;
  output->type = AG_FINGERPRINT_I8;
  output->valid = true;
  return true;
}

bool ag_fingerprint_i16(const int16_t *data, size_t elements,
                        struct ag_data_fingerprint *output)
{
  uint32_t hash = AG_FNV1A_OFFSET;
  int16_t minimum;
  int16_t maximum;
  size_t i;

  if (output == NULL)
    {
      return false;
    }

  memset(output, 0, sizeof(*output));
  if (data == NULL || elements == 0 || elements > UINT32_MAX)
    {
      return false;
    }

  minimum = data[0];
  maximum = data[0];
  for (i = 0; i < elements; i++)
    {
      if (data[i] < minimum)
        {
          minimum = data[i];
        }

      if (data[i] > maximum)
        {
          maximum = data[i];
        }

      hash = ag_fnv1a_u16(hash, (uint16_t)data[i]);
    }

  output->hash = hash;
  output->elements = (uint32_t)elements;
  output->minimum = minimum;
  output->maximum = maximum;
  output->type = AG_FINGERPRINT_I16;
  output->valid = true;
  return true;
}

bool ag_fingerprint_rgb565(const uint16_t *pixels, uint16_t width,
                           uint16_t height,
                           struct ag_rgb565_fingerprint *output)
{
  uint32_t full_hash = AG_FNV1A_OFFSET;
  uint32_t region_hash[3] =
    {
      AG_FNV1A_OFFSET, AG_FNV1A_OFFSET, AG_FNV1A_OFFSET
    };
  size_t pixel_count;
  size_t top_end;
  size_t middle_end;
  size_t i;
  uint16_t minimum;
  uint16_t maximum;

  if (output == NULL)
    {
      return false;
    }

  memset(output, 0, sizeof(*output));
  if (pixels == NULL || width == 0 || height == 0 ||
      (size_t)width > SIZE_MAX / (size_t)height)
    {
      return false;
    }

  pixel_count = (size_t)width * (size_t)height;
  if (pixel_count > UINT32_MAX)
    {
      return false;
    }

  top_end = ((size_t)height / 3) * width;
  middle_end = (((size_t)height * 2) / 3) * width;
  minimum = pixels[0];
  maximum = pixels[0];

  for (i = 0; i < pixel_count; i++)
    {
      unsigned int region = i < top_end ? 0 : i < middle_end ? 1 : 2;

      if (pixels[i] < minimum)
        {
          minimum = pixels[i];
        }

      if (pixels[i] > maximum)
        {
          maximum = pixels[i];
        }

      if (pixels[i] != 0)
        {
          output->nonzero_pixels++;
        }

      full_hash = ag_fnv1a_u16(full_hash, pixels[i]);
      region_hash[region] = ag_fnv1a_u16(region_hash[region], pixels[i]);
    }

  output->full_hash = full_hash;
  output->top_hash = region_hash[0];
  output->middle_hash = region_hash[1];
  output->bottom_hash = region_hash[2];
  output->minimum = minimum;
  output->maximum = maximum;
  output->valid = true;
  return true;
}
