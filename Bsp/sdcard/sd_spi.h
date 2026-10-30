#ifndef BSP_SD_SPI_H
#define BSP_SD_SPI_H

#include <stdint.h>

void sd_spi_init(void);
void sd_spi_cs_select(void);
void sd_spi_cs_deselect(void);
uint8_t sd_spi_xchg(uint8_t data);
void sd_spi_rx(uint8_t *buf, uint32_t len);
void sd_spi_tx(const uint8_t *buf, uint32_t len);
void sd_spi_set_slow(void);
void sd_spi_set_fast(void);

#endif
