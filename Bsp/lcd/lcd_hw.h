#ifndef BSP_LCD_HW_H
#define BSP_LCD_HW_H

#include "stm32f4xx_hal.h"

#define LCD_SPI                 SPI2

#define LCD_SCLK_PORT           GPIOB
#define LCD_SCLK_PIN            GPIO_PIN_13
#define LCD_MOSI_PORT           GPIOB
#define LCD_MOSI_PIN            GPIO_PIN_15
#define LCD_MISO_PORT           GPIOC
#define LCD_MISO_PIN            GPIO_PIN_2

#define LCD_CS_PORT             GPIOB
#define LCD_CS_PIN              GPIO_PIN_9
#define LCD_RESX_PORT           GPIOD
#define LCD_RESX_PIN            GPIO_PIN_10
#define LCD_DCX_PORT            GPIOD
#define LCD_DCX_PIN             GPIO_PIN_9

#define LCD_WIDTH               240U
#define LCD_HEIGHT              320U

#endif
