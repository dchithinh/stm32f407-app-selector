#ifndef BSP_SD_SPI_H
#define BSP_SD_SPI_H

#include <stdint.h>
#include "stm32f4xx_hal.h"

void sd_spi_init(void);
void sd_spi_cs_release_all(void);
void sd_spi_cs_assert(uint16_t cs_pin);
void sd_spi_cs_select(void);
void sd_spi_cs_deselect(void);
uint8_t sd_spi_xchg(uint8_t data);
void sd_spi_rx(uint8_t *buf, uint32_t len);
void sd_spi_tx(const uint8_t *buf, uint32_t len);
void sd_spi_set_slow(void);
void sd_spi_set_fast(void);
void sd_spi_set_touch(void);
void sd_spi_bus_idle_clocks(uint32_t n);

#endif
