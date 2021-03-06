
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * This notice applies to any and all portions of this file
  * that are not between comment pairs USER CODE BEGIN and
  * USER CODE END. Other portions of this file, whether 
  * inserted by the user or by software development tools
  * are owned by their respective copyright owners.
  *
  * Copyright (c) 2020 STMicroelectronics International N.V. 
  * All rights reserved.
  *
  * Redistribution and use in source and binary forms, with or without 
  * modification, are permitted, provided that the following conditions are met:
  *
  * 1. Redistribution of source code must retain the above copyright notice, 
  *    this list of conditions and the following disclaimer.
  * 2. Redistributions in binary form must reproduce the above copyright notice,
  *    this list of conditions and the following disclaimer in the documentation
  *    and/or other materials provided with the distribution.
  * 3. Neither the name of STMicroelectronics nor the names of other 
  *    contributors to this software may be used to endorse or promote products 
  *    derived from this software without specific written permission.
  * 4. This software, including modifications and/or derivative works of this 
  *    software, must execute solely and exclusively on microcontroller or
  *    microprocessor devices manufactured by or for STMicroelectronics.
  * 5. Redistribution and use of this software other than as permitted under 
  *    this license is void and will automatically terminate your rights under 
  *    this license. 
  *
  * THIS SOFTWARE IS PROVIDED BY STMICROELECTRONICS AND CONTRIBUTORS "AS IS" 
  * AND ANY EXPRESS, IMPLIED OR STATUTORY WARRANTIES, INCLUDING, BUT NOT 
  * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A 
  * PARTICULAR PURPOSE AND NON-INFRINGEMENT OF THIRD PARTY INTELLECTUAL PROPERTY
  * RIGHTS ARE DISCLAIMED TO THE FULLEST EXTENT PERMITTED BY LAW. IN NO EVENT 
  * SHALL STMICROELECTRONICS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
  * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, 
  * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF 
  * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING 
  * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
  * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stm32f1xx_hal.h"
#include "usb_device.h"
#include "usbd_cdc_if.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private variables ---------------------------------------------------------*/
SPI_HandleTypeDef hspi1;

TIM_HandleTypeDef htim1;

/* USER CODE BEGIN PV */
/* Private variables ---------------------------------------------------------*/
enum states {
	IDLE,
	WFM_OUT_SINE,
	WFM_OUT_TRIANGLE,
	WFM_OUT_SQUARE,
	WFM_OUT_SAWTOOTH
};

enum waveform_modes {
	SINE, 
	TRIANGLE,
	SQUARE,
	SAWTOOTH
};

struct ad9837_ctrl_reg {
	unsigned int reserved0	: 1;
	unsigned int mode		: 1;
	unsigned int reserved2	: 1;
	unsigned int div2		: 1;
	unsigned int reserved4	: 1;
	unsigned int opbiten	: 1;
	unsigned int sleep12	: 1;
	unsigned int sleep1		: 1;
	unsigned int reset		: 1;
	unsigned int reserved9	: 1;
	unsigned int psel		: 1;
	unsigned int fsel		: 1;
	unsigned int hlb		: 1;
	unsigned int b28		: 1;
	unsigned int freqreg	: 2; 
};

struct ad9837_freq_reg {
	unsigned int freqset	: 14;
	unsigned int freqreg    : 2;
}; 

struct ad9837_phase_reg {
	unsigned int phaseset   : 12;
	unsigned int reserved   : 1;
	unsigned int phasereg   : 3;
}; 

union ad9837_dds_ctrl {
    struct ad9837_ctrl_reg reg;
    uint8_t data[2];
};

union ad9837_freq_set {
	struct ad9837_freq_reg reg;
	uint8_t data[2];
};

union ad9837_phase_set {
	struct ad9837_phase_reg reg;
	uint8_t data[2];
};
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM1_Init(void);
static void MX_SPI1_Init(void);

/* USER CODE BEGIN PFP */
/* Private function prototypes -----------------------------------------------*/
void InitCtrlAD9837(union ad9837_dds_ctrl *dds_control);
void StartOutput(union ad9837_dds_ctrl *dds_control);
void StopOutput(union ad9837_dds_ctrl *dds_control);
void SetFreq0Value(uint32_t freq);
void SetFreq1Value(uint32_t freq);
void SetPhase0Value(uint16_t phase);
void SetPhase1Value(uint16_t phase);
void SetWaveformMode(enum   states			current_state, 
					 union  ad9837_dds_ctrl *dds_control);
enum states NextState(enum states CurrentState);
/* USER CODE END PFP */

/* USER CODE BEGIN 0 */
void delay_us(unsigned int time) 
{
	SysTick->LOAD = time;
	SysTick->VAL  = 0;
	while(!(SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk));
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  *
  * @retval None
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration----------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_TIM1_Init();
  MX_SPI1_Init();
  MX_USB_DEVICE_Init();
  /* USER CODE BEGIN 2 */
  // Following line included to enable SWD debug
  AFIO->MAPR = AFIO_MAPR_SWJ_CFG_JTAGDISABLE;
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  enum states current_state = IDLE;
  union ad9837_dds_ctrl dds_control;

  InitCtrlAD9837(&dds_control);

  // Initializing AD9837 as per ADI App Note AN-1070
  SetFreq0Value(0x00002000);
  SetPhase0Value(0x00FF);

  uint8_t	*head_parse_target  = UserRxBufferFS;
  uint8_t	*tail_parse_target  = UserRxBufferFS;
  uint8_t   parse_buffer[64];
  uint8_t	*head_parse_target  = parse_buffer;
  uint8_t	*tail_parse_target  = parse_buffer;
  uint8_t	usb_packet_flag		= 0;

  while (1)
  {
	GPIOC->BSRR = GPIO_BSRR_BS13;

	/*
	for (int i = 0; i < APP_RX_DATA_SIZE; i++) {
		if (UserRxBufferFS[i] == '@') {
			head_parse_target = &UserRxBufferFS[i];
		}
		if (UserRxBufferFS[i] == '\r') {
			tail_parse_target = &UserRxBufferFS[i];
			CDC_Transmit_FS(head_parse_target, 
							(tail_parse_target-head_parse_target));
		}
	}
	*/

    if (usb_packet_flag) {
        switch(UserRxBufferFS[0]) {
            case 'q':
                current_state = WFM_OUT_SQUARE;
                break;
            case 's':
                current_state = WFM_OUT_SINE;
                break;
            case 't':
                current_state = WFM_OUT_TRIANGLE;
                break;
            case 'i':
                current_state = IDLE;
                break;
            default:
                break;
        }
        usb_packet_flag = 0;
    }
  
	delay_us(48000000);
	current_state = NextState(current_state);

	if (current_state == IDLE) {
		StopOutput(&dds_control);
	} else {
		SetWaveformMode(current_state, &dds_control);
		StartOutput(&dds_control);
	}

	GPIOC->BSRR = GPIO_BSRR_BR13;
	delay_us(48000000);
  /* USER CODE END WHILE */

  /* USER CODE BEGIN 3 */

  }
  /* USER CODE END 3 */

}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{

  RCC_OscInitTypeDef RCC_OscInitStruct;
  RCC_ClkInitTypeDef RCC_ClkInitStruct;
  RCC_PeriphCLKInitTypeDef PeriphClkInit;

    /**Initializes the CPU, AHB and APB busses clocks 
    */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = 16;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI_DIV2;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL12;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

    /**Initializes the CPU, AHB and APB busses clocks 
    */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USB;
  PeriphClkInit.UsbClockSelection = RCC_USBCLKSOURCE_PLL;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

    /**Configure the Systick interrupt time 
    */
  HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq()/1000);

    /**Configure the Systick 
    */
  HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);

  /* SysTick_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(SysTick_IRQn, 0, 0);
}

/* SPI1 init function */
static void MX_SPI1_Init(void)
{

  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_16BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_HIGH;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_HARD_OUTPUT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

}

/* TIM1 init function */
static void MX_TIM1_Init(void)
{

  TIM_ClockConfigTypeDef sClockSourceConfig;
  TIM_MasterConfigTypeDef sMasterConfig;

  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 0;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 0;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim1) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

}

/** Configure pins as 
        * Analog 
        * Input 
        * Output
        * EVENT_OUT
        * EXTI
*/
static void MX_GPIO_Init(void)
{

  GPIO_InitTypeDef GPIO_InitStruct;

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GRN_LED_GPIO_Port, GRN_LED_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : GRN_LED_Pin */
  GPIO_InitStruct.Pin = GRN_LED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GRN_LED_GPIO_Port, &GPIO_InitStruct);

}

/* USER CODE BEGIN 4 */
void InitCtrlAD9837(union ad9837_dds_ctrl *dds_control)
{
	dds_control->reg.freqreg	= 0;
	dds_control->reg.b28		= 1;
	dds_control->reg.hlb		= 0;
	dds_control->reg.fsel		= 0;
	dds_control->reg.psel		= 0;
	dds_control->reg.reserved9  = 0;
	dds_control->reg.reset		= 1;
	dds_control->reg.sleep1		= 0;
	dds_control->reg.sleep12    = 0;
	dds_control->reg.opbiten    = 0;
	dds_control->reg.reserved4  = 0;
	dds_control->reg.div2		= 0;
	dds_control->reg.reserved2  = 0;
	dds_control->reg.mode		= 0;
	dds_control->reg.reserved0  = 0;
}

void SetFreq0Value(uint32_t freq) 
{
	union ad9837_freq_set freq0;
	freq0.reg.freqreg = 1;
	freq0.reg.freqset = (unsigned int)(freq & 0x00003fff);
	// Note: All SPI transactions are len=1, but 
	// require a 2 byte input due to the peripheral being 
	// in 16-bit output mode
	
	// Sets 14 LSBs of Freq0 register
	HAL_SPI_Transmit(&hspi1, freq0.data, 1, 10);
	// Sets 14 MSBs of Freq0 register
	freq0.reg.freqreg = 1;
	freq0.reg.freqset = (unsigned int)((freq & 0x0fffc000) >> 14);
	HAL_SPI_Transmit(&hspi1, freq0.data, 1, 10);
}

void SetFreq1Value(uint32_t freq) 
{
	union ad9837_freq_set freq1;
	freq1.reg.freqreg = 2;
	freq1.reg.freqset = (unsigned int)(freq & 0x00003fff);
	// Note: All SPI transactions are len=1, but 
	// require a 2 byte input due to the peripheral being 
	// in 16-bit output mode
	
	// Sets 14 LSBs of Freq0 register
	HAL_SPI_Transmit(&hspi1, freq1.data, 1, 10);
	// Sets 14 MSBs of Freq0 register
	freq1.reg.freqreg = 1;
	freq1.reg.freqset = (unsigned int)((freq & 0x0fffc000) >> 14);
	HAL_SPI_Transmit(&hspi1, freq1.data, 1, 10);
}

void SetPhase0Value(uint16_t phase)
{
	union ad9837_phase_set phase0;
	phase0.reg.phasereg = 6; // Must be 0x6 (0b110) to set PHASEREG bit
	phase0.reg.reserved = 0;
	phase0.reg.phaseset = (phase & 0x0fff);
	HAL_SPI_Transmit(&hspi1, phase0.data, 1, 10);
}

void SetPhase1Value(uint16_t phase)
{
	union ad9837_phase_set phase1;
	phase1.reg.phasereg = 7; // Must be 0x7 (0b111) to set PHASEREG bit
	phase1.reg.reserved = 0;
	phase1.reg.phaseset = (phase & 0x0fff);
	HAL_SPI_Transmit(&hspi1, phase1.data, 1, 10);
}

void StartOutput(union ad9837_dds_ctrl *dds_control) {
	// Note: All SPI transactions are len=1, but 
	// require a 2 byte input due to the peripheral being 
	// in 16-bit output mode
	dds_control->reg.reset = 0;
	HAL_SPI_Transmit(&hspi1, 
					 dds_control->data, 1, 10);
}

void StopOutput(union ad9837_dds_ctrl *dds_control) 
{
	// Note: All SPI transactions are len=1, but 
	// require a 2 byte input due to the peripheral being 
	// in 16-bit output mode
	dds_control->reg.reset = 1;
	HAL_SPI_Transmit(&hspi1, 
					 dds_control->data, 1, 10);
}

void SetWaveformMode(enum   states			current_state, 
					 union  ad9837_dds_ctrl *dds_control)
{
	switch (current_state) {
		case WFM_OUT_SINE:
			// Clears OPBITEN and MODE bits
			dds_control->reg.opbiten = 0;
			dds_control->reg.mode    = 0;
			// Transmits new settings to DDS via SPI
			HAL_SPI_Transmit(&hspi1, 
							 dds_control->data, 
							 1, 
							 10);
			break;
		case WFM_OUT_TRIANGLE:
			// Sets MODE bit; clears OPBITEN
			dds_control->reg.opbiten = 0;
			dds_control->reg.mode    = 1;
			// Transmits new settings to DDS via SPI
			HAL_SPI_Transmit(&hspi1, 
							 dds_control->data, 
							 1, 
							 10);
			break;
		case WFM_OUT_SQUARE:
			// Sets OPBITEN;
			dds_control->reg.opbiten = 1;
			// Transmits new settings to DDS via SPI
			HAL_SPI_Transmit(&hspi1, 
							 dds_control->data, 
							 1, 
							 10);
			break;
		default:
			break;	
	}
}

enum states NextState(enum states CurrentState)
{
	switch (CurrentState) {
		case IDLE:
			return WFM_OUT_SINE;
			break;
		case WFM_OUT_SINE:
			return WFM_OUT_TRIANGLE;
			break;
		case WFM_OUT_TRIANGLE:
			return IDLE;
			break;
		default:
			return IDLE;
			break;
	}
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @param  file: The file name as string.
  * @param  line: The line in file as a number.
  * @retval None
  */
void _Error_Handler(char *file, int line)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  while(1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t* file, uint32_t line)
{ 
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/**
  * @}
  */

/**
  * @}
  */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
