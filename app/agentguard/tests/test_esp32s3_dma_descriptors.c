/* SPDX-License-Identifier: Apache-2.0 */

#include "esp32s3_spi_psram_dma.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#define TEST_BUFFER_BYTES 65536
#define TEST_DATA_BYTES 60000
#define TEST_DESCRIPTOR_COUNT 15

static uint8_t g_buffer[TEST_BUFFER_BYTES] __attribute__((aligned(64)));

int main(void)
{
  struct esp32s3_dmadesc_s descriptors[TEST_DESCRIPTOR_COUNT];
  uint32_t transferred;
  unsigned int i;

  transferred = esp32s3_spi_psram_dma_setup(descriptors,
                                             TEST_DESCRIPTOR_COUNT,
                                             g_buffer,
                                             TEST_DATA_BYTES);
  assert(transferred == TEST_DATA_BYTES);

  for (i = 0; i < TEST_DESCRIPTOR_COUNT; i++)
    {
      uintptr_t offset = (uintptr_t)descriptors[i].pbuf -
                         (uintptr_t)g_buffer;

      assert((offset % 64) == 0);
    }

  assert(((descriptors[0].ctrl >> ESP32S3_DMA_CTRL_DATALEN_S) &
          ESP32S3_DMA_CTRL_DATALEN_V) == 4032);
  assert(((descriptors[0].ctrl >> ESP32S3_DMA_CTRL_BUFLEN_S) &
          ESP32S3_DMA_CTRL_BUFLEN_V) == 4032);
  assert(((descriptors[14].ctrl >> ESP32S3_DMA_CTRL_DATALEN_S) &
          ESP32S3_DMA_CTRL_DATALEN_V) == 3552);
  assert(((descriptors[14].ctrl >> ESP32S3_DMA_CTRL_BUFLEN_S) &
          ESP32S3_DMA_CTRL_BUFLEN_V) == 3584);
  assert((descriptors[14].ctrl & ESP32S3_DMA_CTRL_EOF) != 0);
  assert(descriptors[14].next == NULL);

  puts("ESP32-S3 PSRAM TX DMA descriptor tests: PASS");
  return 0;
}
