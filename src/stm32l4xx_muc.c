/**
 * Copyright (c) 2015 Motorola.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from this
 * software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stm32_hal_mod.h>
#include <stm32_mod_device.h>

static UART_HandleTypeDef huart;
extern SPI_HandleTypeDef  hspi; /* defined in main */

#define OTP_BASE_ADDR      0x1FFF7000
/* Each OTP block is 8 bytes(double word) */
#define MAX_OTP_BLOCKS  128
#define OTP_BLOCK_SIZE  8
#define MAX_KEY_REVOKES 16

struct otp_registers {
  uint64_t key_revoked[MAX_KEY_REVOKES];
  uint64_t reserved[MAX_OTP_BLOCKS - MAX_KEY_REVOKES];
};

bool mods_is_spi_csn(uint16_t pin)
{
  return (pin == GPIO_PIN_SPI_CS_N);
}

int get_chip_uid(uint64_t *uid_high, uint64_t *uid_low)
{
  uint32_t regval;

  regval = getreg32(STM32_UID_BASE);
  *uid_low = regval;

  regval = getreg32(STM32_UID_BASE + 4);
  *uid_low |= ((uint64_t)regval) << 32;

  regval = getreg32(STM32_UID_BASE + 8);
  *uid_high = regval;

  return 0;
}

/** System Clock Configuration
*/
void SystemClock_Config(void)
{

  RCC_OscInitTypeDef RCC_OscInitStruct;
  RCC_ClkInitTypeDef RCC_ClkInitStruct;
  RCC_PeriphCLKInitTypeDef PeriphClkInit;

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = 16;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 8;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  HAL_RCC_OscConfig(&RCC_OscInitStruct);

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3);

  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART1;
  PeriphClkInit.Usart1ClockSelection = RCC_USART1CLKSOURCE_PCLK2;
  HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit);

  __PWR_CLK_ENABLE();

  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

  HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq()/1000);

  HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);

  /* SysTick_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(SysTick_IRQn, 0, 0);
}


/* USART init function */
void MX_USART_UART_Init(void)
{
  device_console_init();

  huart.Instance = MOD_DEBUG_USART;
  huart.Init.BaudRate = 115200;
  huart.Init.WordLength = UART_WORDLENGTH_8B;
  huart.Init.StopBits = UART_STOPBITS_1;
  huart.Init.Parity = UART_PARITY_NONE;
  huart.Init.Mode = UART_MODE_TX;
  huart.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart.Init.OverSampling = UART_OVERSAMPLING_16;
  huart.Init.OneBitSampling = UART_ONEBIT_SAMPLING_DISABLED;
  huart.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  HAL_UART_Init(&huart);
}

/* SPI init function */
void MX_SPI_Init(void)
{
  hspi.Instance = MOD_TO_BASE_SPI;
  hspi.Init.Mode = SPI_MODE_SLAVE;
  hspi.Init.Direction = SPI_DIRECTION_2LINES;
  hspi.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi.Init.NSS = SPI_NSS_HARD_INPUT;
  hspi.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi.Init.TIMode = SPI_TIMODE_DISABLED;
  hspi.Init.CRCCalculation = SPI_CRCCALCULATION_ENABLED;
  hspi.Init.CRCPolynomial = 0x8005;
  hspi.Init.CRCLength = SPI_CRC_LENGTH_16BIT;
  hspi.Init.NSSPMode = SPI_NSS_PULSE_DISABLED;

  HAL_SPI_Init(&hspi);

  /* Enable Software Slave Management to prevent spurious receives */
  hspi.Instance->CR1 |= SPI_CR1_SSM | SPI_CR1_SSI;
}

/**
  * Enable DMA controller clock
  */
void MX_DMA_Init(void)
{
  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  HAL_NVIC_SetPriority(DMA1_Channel4_IRQn, 4, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel4_IRQn);
  HAL_NVIC_SetPriority(DMA1_Channel5_IRQn, 4, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel5_IRQn);

}

/** Configure pins as 
        * Analog 
        * Input 
        * Output
        * EVENT_OUT
        * EXTI
*/
void MX_GPIO_Init(void)
{

  GPIO_InitTypeDef GPIO_InitStruct;

  /* GPIO Ports Clock Enable */
  mods_gpio_clk_enable();

  /*Configure GPIO pin : MUC_INT */
  GPIO_InitStruct.Pin = GPIO_PIN_MUC_INT;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;
  HAL_GPIO_Init(GPIO_PORT_MUC_INT, &GPIO_InitStruct);

 /*Configure GPIO pin : RDY/RFR */
  GPIO_InitStruct.Pin = GPIO_PIN_RFR;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;
  HAL_GPIO_Init(GPIO_PORT_RFR, &GPIO_InitStruct);

  mods_muc_int_set(PIN_RESET);
  mods_rfr_set(PIN_RESET);

  /*Configure GPIO pin : WAKE_N (input) */
  GPIO_InitStruct.Pin = GPIO_PIN_WAKE_N;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIO_PORT_WAKE_N, &GPIO_InitStruct);

  device_gpio_init();
}

int chip_is_key_revoked(int index)
{
  struct otp_registers *otp_regs;
  uint32_t data;

  otp_regs = (struct otp_registers *)(OTP_BASE_ADDR);
  data = (__IO uint32_t)otp_regs->key_revoked[index];

  dbgprintx32("OTP Revoke: ", data, "\r\n");

  if (data == 0)
  {
    return 1;
  }
  return 0;
}
