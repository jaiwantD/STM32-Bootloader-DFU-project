/////* USER CODE BEGIN Header */
/////**
////  ******************************************************************************
////  * @file           : main.c
////  * @brief          : Main program body
////  ******************************************************************************
////  * @attention
////  *
////  * Copyright (c) 2026 STMicroelectronics.
////  * All rights reserved.
////  *
////  * This software is licensed under terms that can be found in the LICENSE file
////  * in the root directory of this software component.
////  * If no LICENSE file comes with this software, it is provided AS-IS.
////  *
////  ******************************************************************************
////  */
/////* USER CODE END Header */
/////* Includes ------------------------------------------------------------------*/
////#include "main.h"
////
/////* Private includes ----------------------------------------------------------*/
/////* USER CODE BEGIN Includes */
////#include "string.h"
////#define APP_START_ADDRESS 0x08010000 // Sector 4
////#define CHUNK_SIZE        256
////#define UPDATE_TIMEOUT_MS 5000       // Wait 2 seconds for ESP32
/////* USER CODE END Includes */
////
/////* Private typedef -----------------------------------------------------------*/
/////* USER CODE BEGIN PTD */
////typedef void (*pFunction)(void);
/////* USER CODE END PTD */
////
/////* Private define ------------------------------------------------------------*/
/////* USER CODE BEGIN PD */
////
/////* USER CODE END PD */
////
/////* Private macro -------------------------------------------------------------*/
/////* USER CODE BEGIN PM */
////
/////* USER CODE END PM */
////
/////* Private variables ---------------------------------------------------------*/
////UART_HandleTypeDef huart2;
////
/////* USER CODE BEGIN PV */
////
/////* USER CODE END PV */
////
/////* Private function prototypes -----------------------------------------------*/
////void SystemClock_Config(void);
////static void MX_GPIO_Init(void);
////static void MX_USART2_UART_Init(void);
/////* USER CODE BEGIN PFP */
////
/////* USER CODE END PFP */
////
/////* Private user code ---------------------------------------------------------*/
/////* USER CODE BEGIN 0 */
////void Erase_Application_Memory(void) {
////    FLASH_EraseInitTypeDef EraseInitStruct;
////    uint32_t SectorError = 0;
////
////    HAL_FLASH_Unlock();
////    EraseInitStruct.TypeErase     = FLASH_TYPEERASE_SECTORS;
////    EraseInitStruct.VoltageRange  = FLASH_VOLTAGE_RANGE_3;
////    EraseInitStruct.Sector        = FLASH_SECTOR_4;
////    EraseInitStruct.NbSectors     = 4; // Erase Sectors 4, 5, 6, 7
////
////    HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError);
////    HAL_FLASH_Lock();
////}
////
////void Write_Firmware_Chunk(uint32_t start_address, uint8_t *data, uint32_t data_length) {
////    HAL_FLASH_Unlock();
////    for (uint32_t i = 0; i < data_length; i += 4) {
////        // Assemble 4 bytes into a 32-bit word (Little Endian)
////        uint32_t word = data[i] | (data[i+1] << 8) | (data[i+2] << 16) | (data[i+3] << 24);
////        HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, start_address + i, word);
////    }
////    HAL_FLASH_Lock();
////}
////
////void Jump_To_Application(void) {
////    // Check if valid stack pointer in application space
////    if (((*(__IO uint32_t*)APP_START_ADDRESS) & 0x2FFE0000) == 0x20000000) {
////
////        // De-initialize UART and GPIO to leave a clean state for the App
////        HAL_UART_DeInit(&huart2);
////        HAL_GPIO_DeInit(GPIOA, GPIO_PIN_5);
////
////        // Disable SysTick and Interrupts
////        SysTick->CTRL = 0;
////        __disable_irq();
////
////        // Jump to Application
////        uint32_t app_jump_address = *(__IO uint32_t*) (APP_START_ADDRESS + 4);
////        pFunction Jump_To_App = (pFunction)app_jump_address;
////
////        // Initialize user application's Stack Pointer
////        __set_MSP(*(__IO uint32_t*) APP_START_ADDRESS);
////
////        Jump_To_App();
////    }
////}
/////* USER CODE END 0 */
////
/////**
////  * @brief  The application entry point.
////  * @retval int
////  */
////int main(void)
////{
////
////  /* USER CODE BEGIN 1 */
////
////  /* USER CODE END 1 */
////
////  /* MCU Configuration--------------------------------------------------------*/
////
////  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
////  HAL_Init();
////
////  /* USER CODE BEGIN Init */
////
////  /* USER CODE END Init */
////
////  /* Configure the system clock */
////  SystemClock_Config();
////
////  /* USER CODE BEGIN SysInit */
////
////  /* USER CODE END SysInit */
////
////  /* Initialize all configured peripherals */
////  MX_GPIO_Init();
////  MX_USART2_UART_Init();
////  /* USER CODE BEGIN 2 */
////
////  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, 1);
////
////  HAL_UART_Transmit(&huart2, (uint8_t*)"READY", 5, 100);
////
////    uint8_t start_cmd[12] = {0};
////    uint8_t rx_buffer[CHUNK_SIZE] = {0};
////
////  HAL_StatusTypeDef req_status = HAL_UART_Receive(&huart2, start_cmd, 12, UPDATE_TIMEOUT_MS);
////
////  if (req_status == HAL_OK && strncmp((char*)start_cmd, "START_UPDATE", 12) == 0) {
////
////        // 1. Unlock Flash and Erase Sector 4 (Application Space)
////        HAL_FLASH_Unlock();
////        FLASH_EraseInitTypeDef EraseInitStruct;
////        uint32_t SectorError;
////
////        EraseInitStruct.TypeErase = FLASH_TYPEERASE_SECTORS;
////        EraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3;
////        EraseInitStruct.Sector = FLASH_SECTOR_4; // 0x08010000
////        EraseInitStruct.NbSectors = 4;           // Erase sectors 4, 5, 6, 7 (adjust as needed for app size)
////
////        if (HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError) == HAL_OK) {
////            // Send ACK that erase is done
////            HAL_UART_Transmit(&huart2, (uint8_t*)"ACK", 3, 100);
////
////            uint32_t write_address = APP_START_ADDRESS;
////
////            // 2. Receive Firmware Chunks and Write to Flash
////            while (1) {
////                if (HAL_UART_Receive(&huart2, rx_buffer, CHUNK_SIZE, 2000) == HAL_OK) {
////                    // Write chunk to flash word by word
////                    for (int i = 0; i < CHUNK_SIZE; i += 4) {
////                        uint32_t word = rx_buffer[i] | (rx_buffer[i+1] << 8) | (rx_buffer[i+2] << 16) | (rx_buffer[i+3] << 24);
////                        HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, write_address, word);
////                        write_address += 4;
////                    }
////                    // Send ACK for chunk
////                    HAL_UART_Transmit(&huart2, (uint8_t*)"ACK", 3, 100);
////                } else {
////                    // Timeout or Error - Flashing complete or failed
////                    break;
////                }
////            }
////        }
////        HAL_FLASH_Lock();
////    }
////
////    // Turn off LED before jumping
////    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
////
////    // Jump to Application
////    Jump_To_Application();
////
////    /* USER CODE END 2 */
////
////    /* Infinite loop */
////    while (1)
////    {
////        // Should never reach here if application is valid
////    }
////  }
////  /* USER CODE END 3 */
////
////
/////**
////  * @brief System Clock Configuration
////  * @retval None
////  */
////void SystemClock_Config(void)
////{
////  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
////  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
////
////  /** Configure the main internal regulator output voltage
////  */
////  __HAL_RCC_PWR_CLK_ENABLE();
////  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);
////
////  /** Initializes the RCC Oscillators according to the specified parameters
////  * in the RCC_OscInitTypeDef structure.
////  */
////  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
////  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
////  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
////  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
////  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
////  {
////    Error_Handler();
////  }
////
////  /** Initializes the CPU, AHB and APB buses clocks
////  */
////  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
////                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
////  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
////  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
////  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
////  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
////
////  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
////  {
////    Error_Handler();
////  }
////}
////
/////**
////  * @brief USART2 Initialization Function
////  * @param None
////  * @retval None
////  */
////static void MX_USART2_UART_Init(void)
////{
////
////  /* USER CODE BEGIN USART2_Init 0 */
////
////  /* USER CODE END USART2_Init 0 */
////
////  /* USER CODE BEGIN USART2_Init 1 */
////
////  /* USER CODE END USART2_Init 1 */
////  huart2.Instance = USART2;
////  huart2.Init.BaudRate = 115200;
////  huart2.Init.WordLength = UART_WORDLENGTH_8B;
////  huart2.Init.StopBits = UART_STOPBITS_1;
////  huart2.Init.Parity = UART_PARITY_NONE;
////  huart2.Init.Mode = UART_MODE_TX_RX;
////  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
////  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
////  if (HAL_UART_Init(&huart2) != HAL_OK)
////  {
////    Error_Handler();
////  }
////  /* USER CODE BEGIN USART2_Init 2 */
////
////  /* USER CODE END USART2_Init 2 */
////
////}
////
/////**
////  * @brief GPIO Initialization Function
////  * @param None
////  * @retval None
////  */
////static void MX_GPIO_Init(void)
////{
////  GPIO_InitTypeDef GPIO_InitStruct = {0};
////  /* USER CODE BEGIN MX_GPIO_Init_1 */
////
////  /* USER CODE END MX_GPIO_Init_1 */
////
////  /* GPIO Ports Clock Enable */
////  __HAL_RCC_GPIOH_CLK_ENABLE();
////  __HAL_RCC_GPIOA_CLK_ENABLE();
////
////  /*Configure GPIO pin Output Level */
////  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
////
////  /*Configure GPIO pin : PA5 */
////  GPIO_InitStruct.Pin = GPIO_PIN_5;
////  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
////  GPIO_InitStruct.Pull = GPIO_NOPULL;
////  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
////  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
////
////  /* USER CODE BEGIN MX_GPIO_Init_2 */
////
////  /* USER CODE END MX_GPIO_Init_2 */
////}
////
/////* USER CODE BEGIN 4 */
////
/////* USER CODE END 4 */
////
/////**
////  * @brief  This function is executed in case of error occurrence.
////  * @retval None
////  */
////void Error_Handler(void)
////{
////  /* USER CODE BEGIN Error_Handler_Debug */
////  /* User can add his own implementation to report the HAL error return state */
////  __disable_irq();
////  while (1)
////  {
////  }
////  /* USER CODE END Error_Handler_Debug */
////}
////#ifdef USE_FULL_ASSERT
/////**
////  * @brief  Reports the name of the source file and the source line number
////  *         where the assert_param error has occurred.
////  * @param  file: pointer to the source file name
////  * @param  line: assert_param error line source number
////  * @retval None
////  */
////void assert_failed(uint8_t *file, uint32_t line)
////{
////  /* USER CODE BEGIN 6 */
////  /* User can add his own implementation to report the file name and line number,
////     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
////  /* USER CODE END 6 */
////}
////#endif /* USE_FULL_ASSERT */
//
///* USER CODE BEGIN Header */
///**
//  ******************************************************************************
//  * @file           : main.c  (BOOTLOADER - fixed)
//  * @brief          : STM32F446 custom UART bootloader
//  *
//  * Key fixes vs original:
//  * 1. "READY" is sent repeatedly (beacon) every ~1 second instead of once,
//  *    so the ESP32 can catch it regardless of when it starts listening.
//  * 2. UPDATE_TIMEOUT_MS increased to 30s so user has time to press Reset
//  *    after the web upload completes.
//  * 3. Explicit "FLASH_END" command terminates flashing cleanly instead of
//  *    relying on a 2-second UART timeout.
//  * 4. Jump_To_Application() now clears all NVIC interrupt enables before
//  *    jumping, preventing stale IRQs from crashing the app at startup.
//  * 5. Stack pointer validity mask fixed for STM32F446 (128KB RAM).
//  ******************************************************************************
//  */
///* USER CODE END Header */
////
////#include "main.h"
////#include "string.h"
////#include <stdbool.h>
////
/////* ---- Bootloader Configuration ---- */
////#define APP_START_ADDRESS   0x08010000UL   // Sector 4 — matches app linker script
////#define CHUNK_SIZE          256
////#define READY_BEACON_MS     1000           // Send "READY" every 1 second
////#define UPDATE_WAIT_MS      30000          // Wait up to 30s for START_UPDATE
////
/////* ---- Private typedefs ---- */
////typedef void (*pFunction)(void);
////
/////* ---- Private variables ---- */
////UART_HandleTypeDef huart2;
////
/////* ---- Private function prototypes ---- */
////void SystemClock_Config(void);
////static void MX_GPIO_Init(void);
////static void MX_USART2_UART_Init(void);
////
/////* ============================================================
//// * Jump_To_Application
//// * FIX: Clear all NVIC interrupt enables so no stale peripheral
//// * interrupt fires the moment the app calls __enable_irq().
//// * FIX: Validity mask corrected for 128KB RAM (0x20000000–0x2001FFFF).
//// * ============================================================ */
////void Jump_To_Application(void) {
////    uint32_t sp = *(__IO uint32_t*)APP_START_ADDRESS;
////
////    // Validate stack pointer: must be within SRAM (128KB on F446)
////    // 0x20000000 to 0x2001FFFF
////    if ((sp < 0x20000000UL) || (sp > 0x20020000UL)) {
////        // No valid application — stay in bootloader / signal error
////        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);
////        while (1) {
////            HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
////            HAL_Delay(200); // Fast blink = no valid app
////        }
////    }
////
////    // De-initialize peripherals
////    HAL_UART_DeInit(&huart2);
////    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_5);
////
////    // Disable SysTick
////    SysTick->CTRL = 0;
////    SysTick->LOAD = 0;
////    SysTick->VAL  = 0;
////
////    // FIX: Clear all NVIC interrupt enables and pending bits
////    // STM32F446 has 7 NVIC registers (240 external IRQs max, F446 uses ~97)
////    for (int i = 0; i < 8; i++) {
////        NVIC->ICER[i] = 0xFFFFFFFF;   // Disable all
////        NVIC->ICPR[i] = 0xFFFFFFFF;   // Clear all pending
////    }
////
////    __disable_irq();
////    __DSB();
////    __ISB();
////
////    // Set vector table to application
////    SCB->VTOR = APP_START_ADDRESS;
////
////    // Get application Reset_Handler address
////    uint32_t app_reset = *(__IO uint32_t*)(APP_START_ADDRESS + 4);
////    pFunction Jump_To_App = (pFunction)app_reset;
////
////    // Set application stack pointer
////    __set_MSP(sp);
////
////    __enable_irq();
////
////    // Jump!
////    Jump_To_App();
////
////    // Should never reach here
////    while (1) {}
////}
////
/////* ============================================================
//// * main
//// * ============================================================ */
////int main(void)
////{
////    HAL_Init();
////    SystemClock_Config();
////    MX_GPIO_Init();
////    MX_USART2_UART_Init();
////
////    // LED ON = bootloader active
////    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);
////
////    uint8_t rx_buffer[CHUNK_SIZE];
////
////    /* ----------------------------------------------------------
////     * FIX: Send "READY" as a repeated beacon every READY_BEACON_MS.
////     * The ESP32 may not be listening yet (it could still be
////     * finishing the HTTP file-upload transaction). Beaconing
////     * means it will catch the signal whenever it is ready.
////     * Listen for "START_UPDATE" between beacons.
////     * ---------------------------------------------------------- */
////    bool update_requested = false;
////    uint32_t beacon_start = HAL_GetTick();
////    uint32_t wait_start   = HAL_GetTick();
////
////    // Flush any noise on the line before starting
////    __HAL_UART_FLUSH_DRREGISTER(&huart2);
////
////    while (!update_requested && (HAL_GetTick() - wait_start) < UPDATE_WAIT_MS) {
////
////        // Send beacon
////        if ((HAL_GetTick() - beacon_start) >= READY_BEACON_MS) {
////            HAL_UART_Transmit(&huart2, (uint8_t*)"READY", 5, 100);
////            beacon_start = HAL_GetTick();
////        }
////
////        // Non-blocking check: try to receive "START_UPDATE" (12 bytes, 200ms timeout)
////        uint8_t cmd_buf[12] = {0};
////        if (HAL_UART_Receive(&huart2, cmd_buf, 12, 200) == HAL_OK) {
////            if (strncmp((char*)cmd_buf, "START_UPDATE", 12) == 0) {
////                update_requested = true;
////            }
////        }
////    }
////
////    if (!update_requested) {
////        // No update — jump to existing application
////        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
////        Jump_To_Application();
////    }
////
////    /* ----------------------------------------------------------
////     * Update requested — erase application sectors and receive
////     * firmware chunks.
////     * ---------------------------------------------------------- */
////
////    // Erase sectors 4–7 (0x08010000 – 0x080FFFFF, up to 448KB for app)
////    HAL_FLASH_Unlock();
////
////    FLASH_EraseInitTypeDef EraseInitStruct = {0};
////    uint32_t SectorError = 0;
////    EraseInitStruct.TypeErase    = FLASH_TYPEERASE_SECTORS;
////    EraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3;
////    EraseInitStruct.Sector       = FLASH_SECTOR_4;
////    EraseInitStruct.NbSectors    = 4;  // Sectors 4, 5, 6, 7
////
////    if (HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError) != HAL_OK) {
////        // Erase failed — signal error and halt
////        HAL_FLASH_Lock();
////        while (1) {
////            HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
////            HAL_Delay(100);
////        }
////    }
////
////    // ACK the erase — ESP32 will now start sending chunks
////    HAL_UART_Transmit(&huart2, (uint8_t*)"ACK", 3, 100);
////
////    uint32_t write_address = APP_START_ADDRESS;
////
////    // Receive chunks
////    while (1) {
////        HAL_StatusTypeDef status = HAL_UART_Receive(&huart2, rx_buffer, CHUNK_SIZE, 3000);
////
////        if (status == HAL_OK) {
////            // FIX: Check for "FLASH_END" signal embedded in the buffer.
////            // The ESP32 sends this as a plain ASCII string after all chunks.
////            // We check the first 9 bytes; a real firmware chunk will never
////            // start with this ASCII sequence.
////            if (strncmp((char*)rx_buffer, "FLASH_END", 9) == 0) {
////                // All done — ACK and break
////                HAL_UART_Transmit(&huart2, (uint8_t*)"ACK", 3, 100);
////                break;
////            }
////
////            // Write chunk to flash, word by word
////            for (int i = 0; i < CHUNK_SIZE; i += 4) {
////                uint32_t word = (uint32_t)rx_buffer[i]
////                              | ((uint32_t)rx_buffer[i+1] << 8)
////                              | ((uint32_t)rx_buffer[i+2] << 16)
////                              | ((uint32_t)rx_buffer[i+3] << 24);
////                HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, write_address, word);
////                write_address += 4;
////            }
////
////            // ACK the chunk
////            HAL_UART_Transmit(&huart2, (uint8_t*)"ACK", 3, 100);
////
////        } else {
////            // HAL_TIMEOUT or HAL_ERROR — treat as end of transfer.
////            // This is the fallback for compatibility if FLASH_END is not sent.
////            break;
////        }
////    }
////
////    HAL_FLASH_Lock();
////
////    // LED off before jumping
////    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
////
////    // Jump to newly flashed application
////    Jump_To_Application();
////
////    // Should never reach here
////    while (1) {}
////}
////
/////* ============================================================
//// * SystemClock_Config — HSI, no PLL, all dividers = 1
//// * ============================================================ */
////void SystemClock_Config(void)
////{
////    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
////    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
////
////    __HAL_RCC_PWR_CLK_ENABLE();
////    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);
////
////    RCC_OscInitStruct.OscillatorType      = RCC_OSCILLATORTYPE_HSI;
////    RCC_OscInitStruct.HSIState            = RCC_HSI_ON;
////    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
////    RCC_OscInitStruct.PLL.PLLState        = RCC_PLL_NONE;
////    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) Error_Handler();
////
////    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
////                                     | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
////    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_HSI;
////    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
////    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
////    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
////    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK) Error_Handler();
////}
////
/////* ============================================================
//// * MX_USART2_UART_Init — 115200 8N1
//// * ============================================================ */
////static void MX_USART2_UART_Init(void)
////{
////    huart2.Instance          = USART2;
////    huart2.Init.BaudRate     = 115200;
////    huart2.Init.WordLength   = UART_WORDLENGTH_8B;
////    huart2.Init.StopBits     = UART_STOPBITS_1;
////    huart2.Init.Parity       = UART_PARITY_NONE;
////    huart2.Init.Mode         = UART_MODE_TX_RX;
////    huart2.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
////    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
////    if (HAL_UART_Init(&huart2) != HAL_OK) Error_Handler();
////}
////
/////* ============================================================
//// * MX_GPIO_Init — PA5 as output (LED)
//// * ============================================================ */
////static void MX_GPIO_Init(void)
////{
////    GPIO_InitTypeDef GPIO_InitStruct = {0};
////
////    __HAL_RCC_GPIOH_CLK_ENABLE();
////    __HAL_RCC_GPIOA_CLK_ENABLE();
////
////    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
////
////    GPIO_InitStruct.Pin   = GPIO_PIN_5;
////    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
////    GPIO_InitStruct.Pull  = GPIO_NOPULL;
////    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
////    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
////}
////
/////* ============================================================
//// * Error_Handler
//// * ============================================================ */
////void Error_Handler(void)
////{
////    __disable_irq();
////    while (1) {}
////}
////
////#ifdef USE_FULL_ASSERT
////void assert_failed(uint8_t *file, uint32_t line) {}
////#endif
//
//
///* USER CODE BEGIN Header */
///**
//  ******************************************************************************
//  * @file           : main.c  (BOOTLOADER - USART1)
//  * @brief          : STM32F446 custom UART bootloader via USART1 (PA9/PA10)
//  *
//  * UART: USART1 — PA9 (TX), PA10 (RX)
//  * These pins are NOT connected to the ST-Link virtual COM port,
//  * so there is no signal contention.
//  ******************************************************************************
//  */
///* USER CODE END Header */
//
//#include "main.h"
//#include "string.h"
//#include <stdbool.h>
//
///* ---- Bootloader Configuration ---- */
//#define APP_START_ADDRESS   0x08010000UL
//#define CHUNK_SIZE          256
//#define READY_BEACON_MS     1000
//#define UPDATE_WAIT_MS      30000
//
///* ---- Private typedefs ---- */
//typedef void (*pFunction)(void);
//
///* ---- Private variables ---- */
//UART_HandleTypeDef huart1;   // Changed from huart2 to huart1
//
///* ---- Private function prototypes ---- */
//void SystemClock_Config(void);
//static void MX_GPIO_Init(void);
//static void MX_USART1_UART_Init(void);  // Changed
//
///* ============================================================
// * Jump_To_Application
// * ============================================================ */
//void Jump_To_Application(void) {
//    uint32_t sp = *(__IO uint32_t*)APP_START_ADDRESS;
//
//    // Validate SP: must be within STM32F446 SRAM (128KB: 0x20000000–0x2001FFFF)
//    if ((sp < 0x20000000UL) || (sp > 0x20020000UL)) {
//        // No valid application — fast blink forever
//        while (1) {
//            HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
//            HAL_Delay(200);
//        }
//    }
//
//    // De-init peripherals
//    HAL_UART_DeInit(&huart1);
//    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_5);
//    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_9 | GPIO_PIN_10);
//
//    // Disable SysTick
//    SysTick->CTRL = 0;
//    SysTick->LOAD = 0;
//    SysTick->VAL  = 0;
//
//    // Clear all NVIC interrupt enables and pending bits
//    for (int i = 0; i < 8; i++) {
//        NVIC->ICER[i] = 0xFFFFFFFF;
//        NVIC->ICPR[i] = 0xFFFFFFFF;
//    }
//
//    __disable_irq();
//    __DSB();
//    __ISB();
//
//    // Relocate vector table to application
//    SCB->VTOR = APP_START_ADDRESS;
//
//    // Get app Reset_Handler
//    uint32_t app_reset = *(__IO uint32_t*)(APP_START_ADDRESS + 4);
//    pFunction Jump_To_App = (pFunction)app_reset;
//
//    // Set app stack pointer and jump
//    __set_MSP(sp);
//    __enable_irq();
//    Jump_To_App();
//
//    while (1) {}
//}
//
///* ============================================================
// * main
// * ============================================================ */
//int main(void)
//{
//    HAL_Init();
//    SystemClock_Config();
//    MX_GPIO_Init();
//    MX_USART1_UART_Init();
//
//    // LED ON = bootloader active
//    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);
//
//    // Flush any noise on the line
//    __HAL_UART_FLUSH_DRREGISTER(&huart1);
//
//    bool update_requested = false;
//    uint32_t beacon_start = HAL_GetTick();
//    uint32_t wait_start   = HAL_GetTick();
//
//    // Beacon "READY" every 1s, listen for "START_UPDATE" between beacons
//    while (!update_requested && (HAL_GetTick() - wait_start) < UPDATE_WAIT_MS) {
//
//        if ((HAL_GetTick() - beacon_start) >= READY_BEACON_MS) {
//            HAL_UART_Transmit(&huart1, (uint8_t*)"READY", 5, 100);
//            beacon_start = HAL_GetTick();
//        }
//
//        uint8_t cmd_buf[12] = {0};
//        if (HAL_UART_Receive(&huart1, cmd_buf, 12, 200) == HAL_OK) {
//            if (strncmp((char*)cmd_buf, "START_UPDATE", 12) == 0) {
//                update_requested = true;
//            }
//        }
//    }
//
//    if (!update_requested) {
//        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
//        Jump_To_Application();
//    }
//
//    // Erase sectors 4–7
//    HAL_FLASH_Unlock();
//
//    FLASH_EraseInitTypeDef EraseInitStruct = {0};
//    uint32_t SectorError = 0;
//    EraseInitStruct.TypeErase    = FLASH_TYPEERASE_SECTORS;
//    EraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3;
//    EraseInitStruct.Sector       = FLASH_SECTOR_4;
//    EraseInitStruct.NbSectors    = 4;
//
//    if (HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError) != HAL_OK) {
//        HAL_FLASH_Lock();
//        while (1) {
//            HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
//            HAL_Delay(100);
//        }
//    }
//
//    // ACK erase done
//    HAL_UART_Transmit(&huart1, (uint8_t*)"ACK", 3, 100);
//
//    uint32_t write_address = APP_START_ADDRESS;
//    uint8_t  rx_buffer[CHUNK_SIZE];
//
//    // Receive and write firmware chunks
//    while (1) {
//        if (HAL_UART_Receive(&huart1, rx_buffer, CHUNK_SIZE, 3000) == HAL_OK) {
//
//            // Check for end-of-transfer signal
//            if (strncmp((char*)rx_buffer, "FLASH_END", 9) == 0) {
//                HAL_UART_Transmit(&huart1, (uint8_t*)"ACK", 3, 100);
//                break;
//            }
//
//            // Write chunk word by word
//            for (int i = 0; i < CHUNK_SIZE; i += 4) {
//                uint32_t word = (uint32_t)rx_buffer[i]
//                              | ((uint32_t)rx_buffer[i+1] << 8)
//                              | ((uint32_t)rx_buffer[i+2] << 16)
//                              | ((uint32_t)rx_buffer[i+3] << 24);
//                HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, write_address, word);
//                write_address += 4;
//            }
//
//            HAL_UART_Transmit(&huart1, (uint8_t*)"ACK", 3, 100);
//
//        } else {
//            // Timeout fallback — treat as end of transfer
//            break;
//        }
//    }
//
//    HAL_FLASH_Lock();
//    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
//    Jump_To_Application();
//
//    while (1) {}
//}
//
///* ============================================================
// * SystemClock_Config — HSI, no PLL
// * ============================================================ */
//void SystemClock_Config(void)
//{
//    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
//    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
//
//    __HAL_RCC_PWR_CLK_ENABLE();
//    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);
//
//    RCC_OscInitStruct.OscillatorType      = RCC_OSCILLATORTYPE_HSI;
//    RCC_OscInitStruct.HSIState            = RCC_HSI_ON;
//    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
//    RCC_OscInitStruct.PLL.PLLState        = RCC_PLL_NONE;
//    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) Error_Handler();
//
//    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
//                                     | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
//    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_HSI;
//    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
//    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
//    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
//    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK) Error_Handler();
//}
//
///* ============================================================
// * MX_USART1_UART_Init — USART1 on PA9(TX) / PA10(RX), 115200 8N1
// * USART1 is on APB2 — no solder bridge conflict with ST-Link
// * ============================================================ */
//static void MX_USART1_UART_Init(void)
//{
//    huart1.Instance          = USART1;
//    huart1.Init.BaudRate     = 115200;
//    huart1.Init.WordLength   = UART_WORDLENGTH_8B;
//    huart1.Init.StopBits     = UART_STOPBITS_1;
//    huart1.Init.Parity       = UART_PARITY_NONE;
//    huart1.Init.Mode         = UART_MODE_TX_RX;
//    huart1.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
//    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
//    if (HAL_UART_Init(&huart1) != HAL_OK) Error_Handler();
//}
//
///* ============================================================
// * MX_GPIO_Init — PA5 LED + PA9/PA10 handled by HAL_UART_Init
// * ============================================================ */
//static void MX_GPIO_Init(void)
//{
//    GPIO_InitTypeDef GPIO_InitStruct = {0};
//
//    __HAL_RCC_GPIOH_CLK_ENABLE();
//    __HAL_RCC_GPIOA_CLK_ENABLE();
//
//    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
//
//    GPIO_InitStruct.Pin   = GPIO_PIN_5;
//    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
//    GPIO_InitStruct.Pull  = GPIO_NOPULL;
//    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
//    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
//}
//
//void Error_Handler(void)
//{
//    __disable_irq();
//    while (1) {}
//}
//
//#ifdef USE_FULL_ASSERT
//void assert_failed(uint8_t *file, uint32_t line) {}
//#endif
