#include "debug_uart.h"
#include "hat_board.h"
#include "main.h"
#include <stdio.h>

UART_HandleTypeDef huart2;

void debug_uart_init(void)
{
	GPIO_InitTypeDef gpio = {0};

	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_USART2_CLK_ENABLE();

	gpio.Pin = DBG_UART_TX_PIN | DBG_UART_RX_PIN;
	gpio.Mode = GPIO_MODE_AF_PP;
	gpio.Pull = GPIO_PULLUP;
	gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	gpio.Alternate = GPIO_AF7_USART2;
	HAL_GPIO_Init(GPIOA, &gpio);

	huart2.Instance = DBG_UART;
	huart2.Init.BaudRate = 115200;
	huart2.Init.WordLength = UART_WORDLENGTH_8B;
	huart2.Init.StopBits = UART_STOPBITS_1;
	huart2.Init.Parity = UART_PARITY_NONE;
	huart2.Init.Mode = UART_MODE_TX_RX;
	huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	huart2.Init.OverSampling = UART_OVERSAMPLING_16;
	if (HAL_UART_Init(&huart2) != HAL_OK) {
		Error_Handler();
	}
	/* printf without '\n' must reach the terminal (autoboot countdown uses '\r'). */
	setvbuf(stdout, NULL, _IONBF, 0);
}

int __io_putchar(int ch)
{
	uint8_t c = (uint8_t)ch;
	HAL_UART_Transmit(&huart2, &c, 1, HAL_MAX_DELAY);
	return ch;
}

bool debug_uart_getchar_nonblock(uint8_t *out)
{
	if (out == NULL) {
		return false;
	}
	if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_RXNE) == RESET) {
		return false;
	}
	*out = (uint8_t)(huart2.Instance->DR & 0xFFU);
	return true;
}
