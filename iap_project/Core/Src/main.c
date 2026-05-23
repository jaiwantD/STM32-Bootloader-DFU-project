/*
 * stm32_bootloader_main.c  -  Custom UART (IAP) bootloader for STM32F446RE
 * ---------------------------------------------------------------------------
 * Lives at 0x08000000 (sectors 0-1). Receives a raw .bin over USART1 from an
 * ESP32 and programs it into the APPLICATION region at 0x08008000, then jumps
 * to it. No DFU / BOOT0 needed - BOOT0 stays LOW so this code always runs first.
 *
 * HOW TO USE
 *   1. Create an empty STM32CubeIDE project for STM32F446RETx.
 *   2. Replace the generated Core/Src/main.c with THIS file.
 *      (You do NOT need to configure any peripheral in CubeMX - this file sets
 *       up USART1 and PA5 directly. The HAL drivers are enough.)
 *   3. (Recommended) In the linker script (STM32F446RETX_FLASH.ld) limit the
 *      bootloader so it can never overrun the app region:
 *          FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 32K
 *   4. Build & flash with ST-Link as usual.
 *
 * WIRING (both sides are 3.3 V - no level shifter needed)
 *      ESP32 GPIO17 (TX2) ---> STM32 PA10 (USART1_RX)
 *      ESP32 GPIO16 (RX2) <--- STM32 PA9  (USART1_TX)
 *      ESP32 GND ----------------- STM32 GND
 *      (optional) ESP32 GPIO4 ---> STM32 NRST   to auto-reset before flashing
 *
 * PROTOCOL (ESP32 = host, STM32 = target)
 *      host -> 0x7F                              (handshake, sent until ACK)
 *      tgt  -> 0x79 (ACK)
 *      host -> size[4]  little-endian
 *      tgt  -> erases app sectors, then 0x79 (ACK)
 *      loop: host -> up to 256 data bytes
 *            tgt  -> programs them, then 0x79 (ACK)
 *      when all 'size' bytes are received, target jumps to the new app.
 *
 * Clock: runs on the default HSI @ 16 MHz (no SystemClock_Config needed). The
 * application reconfigures the clock itself after the jump.
 * ---------------------------------------------------------------------------
 */

#include "stm32f4xx_hal.h"

/* ---- Memory map -------------------------------------------------------- */
#define APP_ADDRESS        0x08008000U          /* sector 2 start            */
#define FLASH_END_ADDRESS  0x08080000U          /* 512 KB device end         */
#define SRAM_START         0x20000000U
#define SRAM_END           0x20020000U          /* 128 KB SRAM               */

/* ---- Protocol bytes ---------------------------------------------------- */
#define CMD_HANDSHAKE      0x7FU
#define RESP_ACK           0x79U
#define RESP_NACK          0x1FU

#define CHUNK_SIZE         256U
#define BL_WINDOW_MS       5000U   /* time spent waiting for a handshake     */

UART_HandleTypeDef huart1;

/* ====================================================================== */
/*  Peripheral init                                                        */
/* ====================================================================== */
static void GPIO_LED_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitTypeDef g = {0};
    g.Pin   = GPIO_PIN_5;                 /* LD2 / user LED on Nucleo        */
    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &g);
}

static void USART1_Init(void)
{
    __HAL_RCC_USART1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef g = {0};
    g.Pin       = GPIO_PIN_9 | GPIO_PIN_10;   /* PA9 = TX, PA10 = RX         */
    g.Mode      = GPIO_MODE_AF_PP;
    g.Pull      = GPIO_PULLUP;
    g.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    g.Alternate = GPIO_AF7_USART1;
    HAL_GPIO_Init(GPIOA, &g);

    huart1.Instance          = USART1;
    huart1.Init.BaudRate     = 115200;
    huart1.Init.WordLength   = UART_WORDLENGTH_8B;
    huart1.Init.StopBits     = UART_STOPBITS_1;
    huart1.Init.Parity       = UART_PARITY_NONE;
    huart1.Init.Mode         = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart1);
}

/* ====================================================================== */
/*  Small UART helpers                                                     */
/* ====================================================================== */
static void uart_send(uint8_t b)
{
    HAL_UART_Transmit(&huart1, &b, 1, 200);
}

/* ====================================================================== */
/*  Flash helpers                                                          */
/* ====================================================================== */
static uint32_t addr_to_sector(uint32_t addr)
{
    if      (addr < 0x08004000U) return FLASH_SECTOR_0;   /* 16K */
    else if (addr < 0x08008000U) return FLASH_SECTOR_1;   /* 16K */
    else if (addr < 0x0800C000U) return FLASH_SECTOR_2;   /* 16K */
    else if (addr < 0x08010000U) return FLASH_SECTOR_3;   /* 16K */
    else if (addr < 0x08020000U) return FLASH_SECTOR_4;   /* 64K */
    else if (addr < 0x08040000U) return FLASH_SECTOR_5;   /* 128K */
    else if (addr < 0x08060000U) return FLASH_SECTOR_6;   /* 128K */
    else                         return FLASH_SECTOR_7;   /* 128K */
}

static HAL_StatusTypeDef flash_erase_app(uint32_t size)
{
    uint32_t first = addr_to_sector(APP_ADDRESS);
    uint32_t last  = addr_to_sector(APP_ADDRESS + size - 1U);

    FLASH_EraseInitTypeDef e = {0};
    e.TypeErase    = FLASH_TYPEERASE_SECTORS;
    e.Banks        = FLASH_BANK_1;
    e.Sector       = first;
    e.NbSectors    = (last - first) + 1U;
    e.VoltageRange = FLASH_VOLTAGE_RANGE_3;   /* 2.7-3.6 V -> word program   */

    uint32_t err = 0;
    HAL_FLASH_Unlock();
    HAL_StatusTypeDef st = HAL_FLASHEx_Erase(&e, &err);
    HAL_FLASH_Lock();
    return st;
}

/* len must be a multiple of 4 (caller pads the last chunk with 0xFF) */
static HAL_StatusTypeDef flash_write(uint32_t addr, const uint8_t *data, uint32_t len)
{
    HAL_FLASH_Unlock();
    for (uint32_t i = 0; i < len; i += 4U) {
        uint32_t word =  (uint32_t)data[i]
                      | ((uint32_t)data[i + 1] << 8)
                      | ((uint32_t)data[i + 2] << 16)
                      | ((uint32_t)data[i + 3] << 24);
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr + i, word) != HAL_OK) {
            HAL_FLASH_Lock();
            return HAL_ERROR;
        }
    }
    HAL_FLASH_Lock();
    return HAL_OK;
}

/* ====================================================================== */
/*  Jump to application                                                    */
/* ====================================================================== */
static uint8_t app_is_valid(void)
{
    uint32_t sp = *(volatile uint32_t *)APP_ADDRESS;   /* initial stack ptr  */
    return (sp >= SRAM_START && sp <= SRAM_END);       /* not 0xFFFFFFFF      */
}

static void jump_to_app(void)
{
    uint32_t app_sp    = *(volatile uint32_t *)(APP_ADDRESS);
    uint32_t app_entry = *(volatile uint32_t *)(APP_ADDRESS + 4U);

    __disable_irq();

    /* tidy up everything we touched so the app starts from a clean state */
    HAL_UART_DeInit(&huart1);
    HAL_RCC_DeInit();
    HAL_DeInit();
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL  = 0;

    /* point the vector table at the app (the app also sets this itself) */
    SCB->VTOR = APP_ADDRESS;

    __set_MSP(app_sp);
    __DSB();
    __ISB();
    __enable_irq();

    ((void (*)(void))app_entry)();   /* never returns */
    while (1) { }
}

/* ====================================================================== */
/*  Firmware update sequence                                               */
/* ====================================================================== */
static void receive_and_program(void)
{
    uart_send(RESP_ACK);                       /* ack the handshake          */

    /* 1. read the 4-byte size */
    uint8_t szb[4];
    if (HAL_UART_Receive(&huart1, szb, 4, 5000) != HAL_OK) { uart_send(RESP_NACK); return; }
    uint32_t size = (uint32_t)szb[0] | ((uint32_t)szb[1] << 8)
                  | ((uint32_t)szb[2] << 16) | ((uint32_t)szb[3] << 24);

    if (size == 0 || size > (FLASH_END_ADDRESS - APP_ADDRESS)) { uart_send(RESP_NACK); return; }

    /* 2. erase only the sectors we need */
    if (flash_erase_app(size) != HAL_OK) { uart_send(RESP_NACK); return; }
    uart_send(RESP_ACK);                       /* ready for data             */

    /* 3. receive + program in 256-byte chunks */
    uint32_t addr      = APP_ADDRESS;
    uint32_t remaining = size;
    uint8_t  buf[CHUNK_SIZE];

    while (remaining > 0) {
        uint32_t n = (remaining > CHUNK_SIZE) ? CHUNK_SIZE : remaining;

        if (HAL_UART_Receive(&huart1, buf, n, 5000) != HAL_OK) { uart_send(RESP_NACK); return; }

        uint32_t padded = (n + 3U) & ~3U;      /* round up to a word         */
        for (uint32_t i = n; i < padded; i++) buf[i] = 0xFFU;

        if (flash_write(addr, buf, padded) != HAL_OK) { uart_send(RESP_NACK); return; }

        addr      += n;
        remaining -= n;
        uart_send(RESP_ACK);                   /* tell host to send next     */
    }

    HAL_Delay(20);
    jump_to_app();                             /* done - run the new app     */
}

/* ====================================================================== */
/*  main                                                                   */
/* ====================================================================== */
int main(void)
{
    HAL_Init();              /* leaves clock at HSI 16 MHz - fine for the BL  */
    GPIO_LED_Init();
    USART1_Init();

    /* Wait BL_WINDOW_MS for a handshake. Slow-blink the LED while waiting.   */
    uint8_t  b;
    uint8_t  go_update = 0;
    uint32_t t0   = HAL_GetTick();
    uint32_t tled = t0;

    while ((HAL_GetTick() - t0) < BL_WINDOW_MS) {
        if (HAL_UART_Receive(&huart1, &b, 1, 10) == HAL_OK && b == CMD_HANDSHAKE) {
            go_update = 1;
            break;
        }
        if ((HAL_GetTick() - tled) > 150U) {       /* ~3 Hz = "in bootloader" */
            HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
            tled = HAL_GetTick();
        }
    }

    if (go_update) {
        receive_and_program();                 /* jumps to app on success    */
    }

    if (app_is_valid()) {
        jump_to_app();
    }

    /* No handshake and no valid app -> sit here fast-blinking (error state). */
    while (1) {
        HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
        HAL_Delay(80);
    }
}


