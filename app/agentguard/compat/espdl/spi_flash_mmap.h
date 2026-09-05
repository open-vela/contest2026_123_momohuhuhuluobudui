#pragma once

#define SPI_FLASH_MMAP_DATA 0
static inline int spi_flash_mmap_get_free_pages(int memory)
{
  (void)memory;
  return 0;
}
