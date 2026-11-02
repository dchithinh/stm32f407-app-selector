#ifndef BSP_ILI9341_REGS_H
#define BSP_ILI9341_REGS_H

#include <stdint.h>

#define ILI9341_SWRESET             0x01U
#define ILI9341_SLEEP_OUT           0x11U
#define ILI9341_DISPLAY_ON          0x29U
#define ILI9341_CASET               0x2AU
#define ILI9341_RASET               0x2BU
#define ILI9341_GRAM                0x2CU
#define ILI9341_MAC                 0x36U
#define ILI9341_PIXEL_FORMAT        0x3AU
#define ILI9341_FRMCTR1             0xB1U
#define ILI9341_DFC                 0xB6U
#define ILI9341_POWER1              0xC0U
#define ILI9341_POWER2              0xC1U
#define ILI9341_VCOM1               0xC5U
#define ILI9341_VCOM2               0xC7U
#define ILI9341_PGAMMA              0xE0U
#define ILI9341_NGAMMA              0xE1U
#define ILI9341_POWERA              0xCBU
#define ILI9341_POWERB              0xCFU
#define ILI9341_DTCA                0xE8U
#define ILI9341_DTCB                0xEAU
#define ILI9341_POWER_SEQ           0xEDU
#define ILI9341_3GAMMA_EN           0xF2U
#define ILI9341_GAMMA               0x26U
#define ILI9341_PRC                 0xF7U

#endif
