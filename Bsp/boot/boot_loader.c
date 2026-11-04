#include "boot_loader.h"
#include "debug_uart.h"
#include "stm32f4xx_hal.h"
#include <stdio.h>

void boot_mcu_reboot(void)
{
	printf("Rebooting MCU...\r\n");
	HAL_Delay(30);
	__disable_irq();
	HAL_NVIC_SystemReset();
	for (;;) {
	}
}
