/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2022 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

I2C_HandleTypeDef hi2c1;

UART_HandleTypeDef huart2;

/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = { .name = "defaultTask",
		.stack_size = 128 * 4, .priority = (osPriority_t) osPriorityNormal, };
/* Definitions for weightSensing */
osThreadId_t weightSensingHandle;
const osThreadAttr_t weightSensing_attributes = { .name = "weightSensing",
		.stack_size = 128 * 4, .priority = (osPriority_t) osPriorityLow, };
/* Definitions for resetWeight */
osThreadId_t resetWeightHandle;
const osThreadAttr_t resetWeight_attributes = { .name = "resetWeight",
		.stack_size = 128 * 4, .priority = (osPriority_t) osPriorityNormal, };
/* Definitions for ledDisplay */
osThreadId_t ledDisplayHandle;
const osThreadAttr_t ledDisplay_attributes = { .name = "ledDisplay",
		.stack_size = 128 * 4, .priority = (osPriority_t) osPriorityLow, };
/* Definitions for WeightMutex */
osMutexId_t WeightMutexHandle;
const osMutexAttr_t WeightMutex_attributes = { .name = "WeightMutex" };
/* USER CODE BEGIN PV */
const uint32_t AVERAGE_ROUND = 25;
uint32_t base_weight = 0;
uint32_t result_offset = 8000;
uint32_t divider = 105;
uint32_t AD_RES;
uint8_t is_reset = 0;

// Positive difference threshold
const uint32_t POSITIVE_THRESHOLD = 150;

// Negative difference threshold
const uint32_t NEGATIVE_THRESHOLD = 150; // Measurement error in average case is around +-250

// Current captured weight
uint32_t captured_weight = 0;
uint32_t now_weight;

// LDR Threshold
float LDR_MX = 3500;
float LDR_MN = 800;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_I2C1_Init(void);
static void MX_ADC1_Init(void);
void StartDefaultTask(void *argument);
void StartWeightSense(void *argument);
void ResetWeightHandle(void *argument);
void StartWatchLDR(void *argument);

/* USER CODE BEGIN PFP */

/**
 * Use for delay in microsecond (Reference to clock speed) by called assembly instruction NOP
 * Credit to https://github.com/nimaltd/HX711/blob/a5a317e818fee1aa02c729257cff0308e5ac61c0/hx711.c#L10
 */

void hx711_delay_us(uint32_t delay) {
	while (delay > 0) {
		delay--;
		asm("NOP");
	}
}

/**
 *  All credit to
 *	- https://cdn.sparkfun.com/datasheets/Sensors/ForceFlex/hx711_english.pdf
 *	- https://github.com/nimaltd/HX711/blob/master/hx711.c
 */
uint32_t read_weight(uint8_t tuning) {
	uint32_t count = 0;
	uint32_t startTime = HAL_GetTick();
	uint8_t found = 1;
	while (HAL_GPIO_ReadPin(Weight_SDA_GPIO_Port, Weight_SDA_Pin)) {
		osDelay(1);

		// Timeout
		if (HAL_GetTick() - startTime > 150) {
			found = 0;
			break;
		}
	}

	if (!found) {
		return 0;
	}

	for (int i = 0; i < 24; ++i) {
		HAL_GPIO_WritePin(Weight_SCK_GPIO_Port, Weight_SCK_Pin, GPIO_PIN_SET);
		hx711_delay_us(20);

		count <<= 1;

		HAL_GPIO_WritePin(Weight_SCK_GPIO_Port, Weight_SCK_Pin, GPIO_PIN_RESET);
		hx711_delay_us(20);
		if (HAL_GPIO_ReadPin(Weight_SDA_GPIO_Port, Weight_SDA_Pin)
				== GPIO_PIN_SET)
			++count;
	}

	count ^= 0x800000;
	HAL_GPIO_WritePin(Weight_SDA_GPIO_Port, Weight_SDA_Pin, GPIO_PIN_SET);
	hx711_delay_us(20);
	HAL_GPIO_WritePin(Weight_SDA_GPIO_Port, Weight_SDA_Pin, GPIO_PIN_RESET);
	hx711_delay_us(20);

	if (tuning == 1)
		return count;
	if (count < base_weight)
		return 0;
	return (count - base_weight);
}

uint32_t read_weight_average() {
	uint32_t now = 0;
	for (int i = 0; i < AVERAGE_ROUND; ++i) {
		uint32_t nxt = read_weight(0);

		now += nxt;
		osDelay(1);
	}

	if (now <= result_offset)
		return 0;
	return ((now - result_offset) / (AVERAGE_ROUND * divider));
}

void set_zero_weight() {
	uint32_t now = 0;
	for (int i = 0; i < AVERAGE_ROUND; ++i) {
		uint32_t nxt = read_weight(1);
		now += nxt;
		osDelay(1);
		if (nxt == 0) {
			--i;
			continue;
		}
	}

	now /= AVERAGE_ROUND;
	base_weight = now;
}
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void) {
	/* USER CODE BEGIN 1 */

	/* USER CODE END 1 */

	/* MCU Configuration--------------------------------------------------------*/

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
	MX_USART2_UART_Init();
	MX_I2C1_Init();
	MX_ADC1_Init();
	/* USER CODE BEGIN 2 */

	// Set zero weight
	set_zero_weight();
	captured_weight = read_weight_average();
	// ESP Connection Testing
	HAL_StatusTypeDef esp_check;
	while (1) {
		esp_check = HAL_I2C_Slave_Transmit(&hi2c1, "04", 2, 1000);
		esp_check |= HAL_I2C_Slave_Transmit(&hi2c1, "Test", 4, 1000);

		if (esp_check == HAL_OK)
			break;
		HAL_UART_Transmit(&huart2, "Failed to reach esp8266. Retrying\r\n", 35,
				3000);
		HAL_Delay(1000);
	}

	// Set LED on board to tell that Program is ready
	HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET);

	/* USER CODE END 2 */

	/* Init scheduler */
	osKernelInitialize();
	/* Create the mutex(es) */
	/* creation of WeightMutex */
	WeightMutexHandle = osMutexNew(&WeightMutex_attributes);

	/* USER CODE BEGIN RTOS_MUTEX */
	/* add mutexes, ... */
	/* USER CODE END RTOS_MUTEX */

	/* USER CODE BEGIN RTOS_SEMAPHORES */
	/* add semaphores, ... */
	/* USER CODE END RTOS_SEMAPHORES */

	/* USER CODE BEGIN RTOS_TIMERS */
	/* start timers, add new ones, ... */
	/* USER CODE END RTOS_TIMERS */

	/* USER CODE BEGIN RTOS_QUEUES */
	/* add queues, ... */
	/* USER CODE END RTOS_QUEUES */

	/* Create the thread(s) */
	/* creation of defaultTask */
	defaultTaskHandle = osThreadNew(StartDefaultTask, NULL,
			&defaultTask_attributes);

	/* creation of weightSensing */
	weightSensingHandle = osThreadNew(StartWeightSense, NULL,
			&weightSensing_attributes);

	/* creation of resetWeight */
	resetWeightHandle = osThreadNew(ResetWeightHandle, NULL,
			&resetWeight_attributes);

	/* creation of ledDisplay */
	ledDisplayHandle = osThreadNew(StartWatchLDR, NULL, &ledDisplay_attributes);

	/* USER CODE BEGIN RTOS_THREADS */
	/* add threads, ... */
	/* USER CODE END RTOS_THREADS */

	/* USER CODE BEGIN RTOS_EVENTS */
	/* add events, ... */
	/* USER CODE END RTOS_EVENTS */

	/* Start scheduler */
	osKernelStart();

	/* We should never get here as control is now taken by the scheduler */
	/* Infinite loop */
	/* USER CODE BEGIN WHILE */
	while (1) {
		/* USER CODE END WHILE */

		/* USER CODE BEGIN 3 */
	}
	/* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void) {
	RCC_OscInitTypeDef RCC_OscInitStruct = { 0 };
	RCC_ClkInitTypeDef RCC_ClkInitStruct = { 0 };

	/** Configure the main internal regulator output voltage
	 */
	__HAL_RCC_PWR_CLK_ENABLE();
	__HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
	/** Initializes the RCC Oscillators according to the specified parameters
	 * in the RCC_OscInitTypeDef structure.
	 */
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
	RCC_OscInitStruct.HSIState = RCC_HSI_ON;
	RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
	RCC_OscInitStruct.PLL.PLLM = 8;
	RCC_OscInitStruct.PLL.PLLN = 50;
	RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
	RCC_OscInitStruct.PLL.PLLQ = 4;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
		Error_Handler();
	}
	/** Initializes the CPU, AHB and APB buses clocks
	 */
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
			| RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK) {
		Error_Handler();
	}
}

/**
 * @brief ADC1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_ADC1_Init(void) {

	/* USER CODE BEGIN ADC1_Init 0 */

	/* USER CODE END ADC1_Init 0 */

	ADC_ChannelConfTypeDef sConfig = { 0 };

	/* USER CODE BEGIN ADC1_Init 1 */

	/* USER CODE END ADC1_Init 1 */
	/** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
	 */
	hadc1.Instance = ADC1;
	hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV2;
	hadc1.Init.Resolution = ADC_RESOLUTION_12B;
	hadc1.Init.ScanConvMode = DISABLE;
	hadc1.Init.ContinuousConvMode = DISABLE;
	hadc1.Init.DiscontinuousConvMode = DISABLE;
	hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
	hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
	hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
	hadc1.Init.NbrOfConversion = 1;
	hadc1.Init.DMAContinuousRequests = DISABLE;
	hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
	if (HAL_ADC_Init(&hadc1) != HAL_OK) {
		Error_Handler();
	}
	/** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
	 */
	sConfig.Channel = ADC_CHANNEL_4;
	sConfig.Rank = 1;
	sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;
	if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN ADC1_Init 2 */

	/* USER CODE END ADC1_Init 2 */

}

/**
 * @brief I2C1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_I2C1_Init(void) {

	/* USER CODE BEGIN I2C1_Init 0 */

	/* USER CODE END I2C1_Init 0 */

	/* USER CODE BEGIN I2C1_Init 1 */

	/* USER CODE END I2C1_Init 1 */
	hi2c1.Instance = I2C1;
	hi2c1.Init.ClockSpeed = 100000;
	hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
	hi2c1.Init.OwnAddress1 = 2;
	hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
	hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
	hi2c1.Init.OwnAddress2 = 0;
	hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
	hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
	if (HAL_I2C_Init(&hi2c1) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN I2C1_Init 2 */

	/* USER CODE END I2C1_Init 2 */

}

/**
 * @brief USART2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_USART2_UART_Init(void) {

	/* USER CODE BEGIN USART2_Init 0 */

	/* USER CODE END USART2_Init 0 */

	/* USER CODE BEGIN USART2_Init 1 */

	/* USER CODE END USART2_Init 1 */
	huart2.Instance = USART2;
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
	/* USER CODE BEGIN USART2_Init 2 */

	/* USER CODE END USART2_Init 2 */

}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void) {
	GPIO_InitTypeDef GPIO_InitStruct = { 0 };

	/* GPIO Ports Clock Enable */
	__HAL_RCC_GPIOC_CLK_ENABLE();
	__HAL_RCC_GPIOH_CLK_ENABLE();
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(GPIOA, LD2_Pin | Weight_SCK_Pin, GPIO_PIN_RESET);

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);

	/*Configure GPIO pin : B1_Pin */
	GPIO_InitStruct.Pin = B1_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pins : LD2_Pin Weight_SCK_Pin */
	GPIO_InitStruct.Pin = LD2_Pin | Weight_SCK_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	/*Configure GPIO pin : LED_Pin */
	GPIO_InitStruct.Pin = LED_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(LED_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pin : Weight_SDA_Pin */
	GPIO_InitStruct.Pin = Weight_SDA_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	HAL_GPIO_Init(Weight_SDA_GPIO_Port, &GPIO_InitStruct);

}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/* USER CODE BEGIN Header_StartDefaultTask */
/**
 * @brief  Function implementing the defaultTask thread.
 * @param  argument: Not used
 * @retval None
 */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument) {
	/* USER CODE BEGIN 5 */
	/* Infinite loop */
	for (;;) {
		osDelay(1);
	}
	/* USER CODE END 5 */
}

/* USER CODE BEGIN Header_StartWeightSense */
/**
 * @brief Function implementing the weightSensing thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_StartWeightSense */
void StartWeightSense(void *argument) {
	/* USER CODE BEGIN StartWeightSense */

	/* Infinite loop */
	for (;;) {
		/**
		 * Weight sensing part
		 */
		if (is_reset) {
			osDelay(20);
			continue;
		}
		now_weight = read_weight_average();
		uint8_t is_transmit_data = 0, is_set_data = 0;
		if (now_weight > captured_weight
				&& now_weight - captured_weight >= POSITIVE_THRESHOLD) {
			osDelay(50);
			now_weight = read_weight_average();
			if (now_weight > captured_weight
					&& now_weight - captured_weight >= POSITIVE_THRESHOLD) {
				is_transmit_data = 1;
				is_set_data = 1;
			}
		} else if (now_weight < captured_weight
				&& captured_weight - now_weight >= NEGATIVE_THRESHOLD) {
			// There may have some of measurement delay when place object
			osDelay(50);
			now_weight = read_weight_average();
			if (now_weight < captured_weight
					&& captured_weight - now_weight >= NEGATIVE_THRESHOLD) {
				is_set_data = 1;
			}
		}

		/**
		 * Transmit data to esp, the transfer data process has 2 steps
		 * 1) Send data length
		 * 2) Send data content
		 */
		if (is_set_data && !is_reset) {
			uint32_t tmp = captured_weight;
			captured_weight = now_weight;
			if (is_transmit_data) {
				uint8_t ch[30] = "";
				uint8_t szch[2] = "";

				int sz = sprintf(ch, "%d", captured_weight - tmp);

				if (sz < 10) {
					sprintf(szch, "0%d", sz);
				} else {
					sprintf(szch, "%d", sz);
				}

				HAL_StatusTypeDef result_sz = HAL_I2C_Slave_Transmit(&hi2c1,
						szch, 2, 5000);
				if (result_sz == HAL_OK) {
					HAL_I2C_Slave_Transmit(&hi2c1, ch, sz, 5000);
				}
			}
		}
		osDelay(20);
	}
	/* USER CODE END StartWeightSense */
}

/* USER CODE BEGIN Header_ResetWeightHandle */
/**
 * @brief Function implementing the resetWeight thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_ResetWeightHandle */
void ResetWeightHandle(void *argument) {
	/* USER CODE BEGIN ResetWeightHandle */
	/* Infinite loop */
	/**
	 * Reset button (on board)
	 */
	for (;;) {
		GPIO_PinState btn = HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin);
		if (btn == GPIO_PIN_RESET) {
			is_reset = 1;
			HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);

			osDelay(10);
			set_zero_weight();

			captured_weight = read_weight_average();
			while (HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin) == GPIO_PIN_RESET)
				osDelay(20);
			HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET);
			is_reset = 0;
		}
		osDelay(10);
	}
	/* USER CODE END ResetWeightHandle */
}

/* USER CODE BEGIN Header_StartWatchLDR */
/**
 * @brief Function implementing the ledDisplay thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_StartWatchLDR */
void StartWatchLDR(void *argument) {
	/* USER CODE BEGIN StartWatchLDR */
	/* Infinite loop */
	/**
	 * ADC Part (For turn on light at night)
	 */
	for (;;) {
		HAL_ADC_Start(&hadc1);
		HAL_ADC_PollForConversion(&hadc1, 1);
		AD_RES = HAL_ADC_GetValue(&hadc1);

		float tmp = AD_RES;
		float calC = (1.0 - (LDR_MX - tmp) / (LDR_MX - LDR_MN));

		LDR_MX = tmp > LDR_MX ? tmp : LDR_MX;
		LDR_MN = tmp < LDR_MN ? tmp : LDR_MN;

		if (calC < 0.35) {
			HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);
		} else {
			HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
		}

		osDelay(3000);
	}
	/* USER CODE END StartWatchLDR */
}

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void) {
	/* USER CODE BEGIN Error_Handler_Debug */
	/* User can add his own implementation to report the HAL error return state */
	__disable_irq();
	while (1) {
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
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

