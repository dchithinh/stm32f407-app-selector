#include "sd_spi.h"
#include "hat_board.h"
#include "main.h"

SPI_HandleTypeDef hspi3;

static void sd_gpio_init(void)
{
	GPIO_InitTypeDef gpio = {0};

	__HAL_RCC_GPIOE_CLK_ENABLE();
	__HAL_RCC_GPIOC_CLK_ENABLE();

	HAL_GPIO_WritePin(TOUCH_CS_PORT, TOUCH_CS_PIN, GPIO_PIN_SET);
	HAL_GPIO_WritePin(SD_CS_PORT, SD_CS_PIN, GPIO_PIN_SET);

	gpio.Pin = TOUCH_CS_PIN | SD_CS_PIN;
	gpio.Mode = GPIO_MODE_OUTPUT_PP;
	gpio.Pull = GPIO_NOPULL;
	gpio.Speed = GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(GPIOE, &gpio);

	gpio.Pin = SD_SCK_PIN | SD_MOSI_PIN;
	gpio.Mode = GPIO_MODE_AF_PP;
	gpio.Pull = GPIO_NOPULL;
	gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	gpio.Alternate = GPIO_AF6_SPI3;
	HAL_GPIO_Init(GPIOC, &gpio);

	gpio.Pin = SD_MISO_PIN;
	gpio.Mode = GPIO_MODE_AF_PP;
	gpio.Pull = GPIO_PULLUP;
	gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	gpio.Alternate = GPIO_AF6_SPI3;
	HAL_GPIO_Init(SD_MISO_PORT, &gpio);
}

void sd_spi_init(void)
{
	sd_gpio_init();

	if (hspi3.Instance == SPI3) {
		(void)HAL_SPI_DeInit(&hspi3);
	}

	__HAL_RCC_SPI3_FORCE_RESET();
	__HAL_RCC_SPI3_RELEASE_RESET();

	hspi3.Instance = SD_SPI;
	hspi3.Init.Mode = SPI_MODE_MASTER;
	hspi3.Init.Direction = SPI_DIRECTION_2LINES;
	hspi3.Init.DataSize = SPI_DATASIZE_8BIT;
	hspi3.Init.CLKPolarity = SPI_POLARITY_LOW;
	hspi3.Init.CLKPhase = SPI_PHASE_1EDGE;
	hspi3.Init.NSS = SPI_NSS_SOFT;
	hspi3.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_256;
	hspi3.Init.FirstBit = SPI_FIRSTBIT_MSB;
	hspi3.Init.TIMode = SPI_TIMODE_DISABLE;
	hspi3.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
	hspi3.Init.CRCPolynomial = 10;

	if (HAL_SPI_Init(&hspi3) != HAL_OK) {
		Error_Handler();
	}
	/* CubeMX leaves PC10/PC12 as I2S3 AF6; force SPI mode on the peripheral. */
	CLEAR_BIT(hspi3.Instance->I2SCFGR, SPI_I2SCFGR_I2SMOD);
	__HAL_SPI_ENABLE(&hspi3);
}

void HAL_SPI_MspInit(SPI_HandleTypeDef *hspi)
{
	if (hspi->Instance == SPI3) {
		__HAL_RCC_SPI3_CLK_ENABLE();
		return;
	}

	if (hspi->Instance == SPI2) {
		GPIO_InitTypeDef gpio = {0};

		__HAL_RCC_SPI2_CLK_ENABLE();
		gpio.Pin = GPIO_PIN_13 | GPIO_PIN_15;
		gpio.Mode = GPIO_MODE_AF_PP;
		gpio.Pull = GPIO_NOPULL;
		gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
		gpio.Alternate = GPIO_AF5_SPI2;
		HAL_GPIO_Init(GPIOB, &gpio);

		gpio.Pin = GPIO_PIN_2;
		gpio.Pull = GPIO_PULLUP;
		HAL_GPIO_Init(GPIOC, &gpio);
	}
}

void sd_spi_cs_release_all(void)
{
	HAL_GPIO_WritePin(TOUCH_CS_PORT, TOUCH_CS_PIN, GPIO_PIN_SET);
	HAL_GPIO_WritePin(SD_CS_PORT, SD_CS_PIN, GPIO_PIN_SET);
}

void sd_spi_cs_assert(uint16_t cs_pin)
{
	HAL_GPIO_WritePin(TOUCH_CS_PORT, TOUCH_CS_PIN,
	                  cs_pin == TOUCH_CS_PIN ? GPIO_PIN_RESET : GPIO_PIN_SET);
	HAL_GPIO_WritePin(SD_CS_PORT, SD_CS_PIN,
	                  cs_pin == SD_CS_PIN ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

void sd_spi_cs_select(void)
{
	sd_spi_cs_assert(SD_CS_PIN);
}

void sd_spi_cs_deselect(void)
{
	sd_spi_cs_release_all();
}

uint8_t sd_spi_xchg(uint8_t data)
{
	uint8_t rx = 0xFF;

	if (HAL_SPI_TransmitReceive(&hspi3, &data, &rx, 1, HAL_MAX_DELAY) != HAL_OK) {
		return 0xFF;
	}
	return rx;
}

void sd_spi_rx(uint8_t *buf, uint32_t len)
{
	while (len--) {
		*buf++ = sd_spi_xchg(0xFF);
	}
}

void sd_spi_tx(const uint8_t *buf, uint32_t len)
{
	(void)HAL_SPI_Transmit(&hspi3, (uint8_t *)buf, len, HAL_MAX_DELAY);
}

void sd_spi_set_slow(void)
{
	__HAL_SPI_DISABLE(&hspi3);
	MODIFY_REG(hspi3.Instance->CR1, SPI_CR1_BR, SPI_BAUDRATEPRESCALER_256);
	__HAL_SPI_ENABLE(&hspi3);
}

void sd_spi_set_fast(void)
{
	__HAL_SPI_DISABLE(&hspi3);
	MODIFY_REG(hspi3.Instance->CR1, SPI_CR1_BR, SPI_BAUDRATEPRESCALER_8);
	__HAL_SPI_ENABLE(&hspi3);
}

void sd_spi_set_touch(void)
{
	__HAL_SPI_DISABLE(&hspi3);
	MODIFY_REG(hspi3.Instance->CR1, SPI_CR1_BR, SPI_BAUDRATEPRESCALER_16);
	__HAL_SPI_ENABLE(&hspi3);
}

void sd_spi_bus_idle_clocks(uint32_t n)
{
	sd_spi_cs_deselect();
	while (n--) {
		(void)sd_spi_xchg(0xFF);
	}
}
