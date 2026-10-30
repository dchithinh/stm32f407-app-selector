#ifndef BSP_HAT_BOARD_H
#define BSP_HAT_BOARD_H

#include "stm32f4xx_hal.h"

/* stm32f407_tft_2.4inch_hat — microSD on SPI3 (shared bus with touch) */

#define SD_SPI                  SPI3
#define SD_SPI_HANDLE           hspi3

#define SD_SCK_PORT             GPIOC
#define SD_SCK_PIN              GPIO_PIN_10
#define SD_MISO_PORT            GPIOC
#define SD_MISO_PIN             GPIO_PIN_11
#define SD_MOSI_PORT            GPIOC
#define SD_MOSI_PIN             GPIO_PIN_12

#define SD_CS_PORT              GPIOE
#define SD_CS_PIN               GPIO_PIN_4

#define TOUCH_CS_PORT           GPIOE
#define TOUCH_CS_PIN            GPIO_PIN_3

#define DBG_UART                USART2
#define DBG_UART_HANDLE         huart2
#define DBG_UART_TX_PORT        GPIOA
#define DBG_UART_TX_PIN         GPIO_PIN_2
#define DBG_UART_RX_PORT        GPIOA
#define DBG_UART_RX_PIN         GPIO_PIN_3

extern SPI_HandleTypeDef SD_SPI_HANDLE;
extern UART_HandleTypeDef DBG_UART_HANDLE;

#endif
