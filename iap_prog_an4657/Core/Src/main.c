/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
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
#include <stdio.h>
#include <string.h>
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
#define APP_ADDR        0x08010000U
#define FW_TIMEOUT      15000U
#define BUF_SIZE        256U
#define START_MAGIC     "START"
#define START_LEN       5U
#define ACK_BYTE        0xAA
#define BLOCK_ACK       0x55
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
uint8_t rxBuf[BUF_SIZE];
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */
static uint32_t crc32_update(uint32_t crc, uint8_t data);
static uint32_t crc32_buffer(const uint8_t *data, uint32_t len);
static void JumpToApp(void);
static uint8_t ReceiveFW(void);
static uint8_t AppIsValid(void);
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
int _write(int file, char *ptr, int len)
{
    (void)file;
    HAL_UART_Transmit(&huart1, (uint8_t *)ptr, len, HAL_MAX_DELAY);
    return len;
}

static uint32_t crc32_update(uint32_t crc, uint8_t data)
{
    crc ^= data;
    for (int i = 0; i < 8; i++)
    {
        if (crc & 1U)
            crc = (crc >> 1) ^ 0xEDB88320U;
        else
            crc >>= 1;
    }
    return crc;
}

static uint32_t crc32_buffer(const uint8_t *data, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFFU;
    for (uint32_t i = 0; i < len; i++)
        crc = crc32_update(crc, data[i]);
    return ~crc;
}

static uint8_t AppIsValid(void)
{
    uint32_t sp = *(uint32_t *)APP_ADDR;
    uint32_t rh = *(uint32_t *)(APP_ADDR + 4U);

    if ((sp & 0x2FFE0000U) != 0x20000000U)
        return 0;

    if (rh < APP_ADDR || rh > 0x08100000U)
        return 0;

    return 1;
}

static void JumpToApp(void)
{
    if (!AppIsValid())
        return;

    uint32_t appStack = *(uint32_t *)APP_ADDR;
    uint32_t appReset = *(uint32_t *)(APP_ADDR + 4U);
    void (*appEntry)(void) = (void (*)(void))appReset;

    HAL_UART_DeInit(&huart1);
    HAL_DeInit();

    __disable_irq();

    for (int i = 0; i < 8; i++)
    {
        NVIC->ICER[i] = 0xFFFFFFFFU;
        NVIC->ICPR[i] = 0xFFFFFFFFU;
    }

    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL = 0;

    SCB->VTOR = APP_ADDR;
    __set_MSP(appStack);

    appEntry();
}

static uint8_t ReceiveFW(void)
{
    uint8_t start[START_LEN];
    uint32_t size = 0;
    uint32_t crcRx = 0;
    uint32_t addr = APP_ADDR;
    uint32_t crcCalc = 0xFFFFFFFFU;
    FLASH_EraseInitTypeDef eraseInit = {0};
    uint32_t sectorError = 0;

    memset(rxBuf, 0, sizeof(rxBuf));

    printf("WAIT START\r\n");

    if (HAL_UART_Receive(&huart1, start, START_LEN, FW_TIMEOUT) != HAL_OK)
        return 0;

    if (memcmp(start, START_MAGIC, START_LEN) != 0)
        return 0;

    HAL_UART_Transmit(&huart1, (uint8_t *)&(uint8_t){ACK_BYTE}, 1, HAL_MAX_DELAY);

    if (HAL_UART_Receive(&huart1, (uint8_t *)&size, 4, FW_TIMEOUT) != HAL_OK)
        return 0;

    if (HAL_UART_Receive(&huart1, (uint8_t *)&crcRx, 4, FW_TIMEOUT) != HAL_OK)
        return 0;

    if (size == 0U || size > (448U * 1024U))
        return 0;

    HAL_FLASH_Unlock();

    eraseInit.TypeErase = FLASH_TYPEERASE_SECTORS;
    eraseInit.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    eraseInit.Sector = FLASH_SECTOR_4;
    eraseInit.NbSectors = 4;

    if (HAL_FLASHEx_Erase(&eraseInit, &sectorError) != HAL_OK)
    {
        HAL_FLASH_Lock();
        return 0;
    }

    while (size > 0U)
    {
        uint32_t chunk = (size > BUF_SIZE) ? BUF_SIZE : size;

        if (HAL_UART_Receive(&huart1, rxBuf, chunk, FW_TIMEOUT) != HAL_OK)
        {
            HAL_FLASH_Lock();
            return 0;
        }

        for(
           uint32_t i=0;
           i<chunk;
           i++)
        {
           crcCalc=
           crc32_update(
           crcCalc,
           rxBuf[i]);
        }

        uint32_t padded = chunk;
        while (padded % 4U)
            rxBuf[padded++] = 0xFFU;

        for (uint32_t i = 0; i < padded; i += 4U)
        {
            uint32_t word = *(uint32_t *)&rxBuf[i];

            if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr, word) != HAL_OK)
            {
                HAL_FLASH_Lock();
                return 0;
            }

            addr += 4U;
        }

        HAL_UART_Transmit(&huart1, (uint8_t *)&(uint8_t){BLOCK_ACK}, 1, HAL_MAX_DELAY);
        size -= chunk;
    }

    HAL_FLASH_Lock();


    crcCalc=~crcCalc;

    if(
       crcCalc
       !=
       crcRx)
    {
        return 0;
    }

    return 1;
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

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
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */

  printf("BOOT\r\n");


  HAL_Delay(1000);

  if (ReceiveFW())
  {
      printf("FW OK\r\n");
      HAL_Delay(500);
      NVIC_SystemReset();
  }

  JumpToApp();


  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
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
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);

  /*Configure GPIO pin : PC13 */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : PA5 */
  GPIO_InitStruct.Pin = GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
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
