/* =============================================================================
 * STM32F410RBT6 – USB CDC Bootloader
 * main.c
 *
 * Device facts (from RM0401 + datasheet):
 *   Flash  : 128 KB  (Sectors 0-4)
 *   SRAM   : 32  KB  (0x20000000 – 0x20007FFF)
 *   CPU    : Cortex-M4 @ up to 100 MHz
 *   USB    : USB_OTG_FS on PA11 (DM) / PA12 (DP)
 *
 * Memory layout used by this bootloader:
 *   Sector 0  0x08000000  16 KB  ← bootloader (this code)
 *   Sector 1  0x08004000  16 KB  ┐
 *   Sector 2  0x08008000  16 KB  │ application (sectors 1-4 = 112 KB)
 *   Sector 3  0x0800C000  16 KB  │
 *   Sector 4  0x08010000  64 KB  ┘
 *
 * Clock: HSE 8 MHz → PLL → SYSCLK 96 MHz, USB 48 MHz
 *   PLLM=8, PLLN=192, PLLP=DIV2(96 MHz), PLLQ=4(48 MHz)
 *
 * GPIO used:
 *   PA0  – force-update button (active LOW, internal pull-up)
 *   PC13 – status LED         (active LOW, common on custom boards)
 *   PA11 – USB_OTG_FS_DM      (handled by USB HAL, no explicit init needed)
 *   PA12 – USB_OTG_FS_DP      (handled by USB HAL, no explicit init needed)
 * =============================================================================
 */

#include "main.h"
#include "usb_device.h"
#include "usbd_cdc_if.h"
#include <string.h>
#include <stdio.h>

/* ── Memory map ────────────────────────────────────────────────────────────── */
#define BOOTLOADER_BASE     0x08000000UL   /* Sector 0 – this code           */
#define APP_START_ADDR      0x08010000UL   /* Sector 1 – application IVT     */

/* ── Flash sector definitions for STM32F410RBT6 (128 KB, 5 sectors) ─────── */
/*   Sector 0  0x08000000  16 KB  bootloader                                 */
/*   Sector 1  0x08004000  16 KB  app                                        */
/*   Sector 2  0x08008000  16 KB  app                                        */
/*   Sector 3  0x0800C000  16 KB  app                                        */
/*   Sector 4  0x08010000  64 KB  app   ← LAST SECTOR                       */
#define APP_FIRST_SECTOR    FLASH_SECTOR_1
#define APP_LAST_SECTOR     FLASH_SECTOR_4
#define APP_SECTOR_COUNT    (APP_LAST_SECTOR - APP_FIRST_SECTOR + 1)  /* = 4 */

/* ── SRAM validity window for stack pointer check ──────────────────────────
 *   STM32F410RBT6 has 32 KB SRAM: 0x20000000 – 0x20007FFF
 *   We check a slightly generous upper bound to allow the SP to sit at
 *   the very top of SRAM.
 * ─────────────────────────────────────────────────────────────────────────── */
#define SRAM_BASE_ADDR      0x20000000UL
#define SRAM_END_ADDR       0x20008000UL   /* exclusive upper bound (32 KB)  */

/* ── Protocol bytes ─────────────────────────────────────────────────────── */
#define ACK                 0x06
#define NAK                 0x15
#define EOT                 0x04
#define CMD_INFO            'I'
#define CMD_ERASE           'E'
#define CMD_WRITE           'W'
#define CMD_JUMP            'J'

/* ── Boot timeout ──────────────────────────────────────────────────────────
 *   If no host connects within this many ms and a valid app exists,
 *   we boot the app automatically.
 * ─────────────────────────────────────────────────────────────────────────── */
#define BOOT_TIMEOUT_MS     5000U

/* ── Globals ─────────────────────────────────────────────────────────────── */
extern USBD_HandleTypeDef hUsbDeviceFS;

volatile uint8_t  usb_rx_buf[512];
volatile uint16_t usb_rx_len  = 0;
volatile uint8_t  rx_ready    = 0;

static uint32_t   write_ptr;               /* Next flash write address       */
static uint8_t    update_mode = 0;         /* Set after successful erase     */

/* ── Forward declarations ────────────────────────────────────────────────── */
static void              SystemClock_Config(void);
static int               is_valid_app(void);
static void              jump_to_app(void);
static void              send_byte(uint8_t b);
static void              send_str(const char *s);
static HAL_StatusTypeDef flash_erase_app(void);
static HAL_StatusTypeDef flash_write_chunk(uint32_t addr,
                                            const uint8_t *data,
                                            uint16_t len);
static void              process_command(void);
static uint8_t           crc8_calc(const uint8_t *buf, uint16_t len);

/* =============================================================================
 *  main()
 * ============================================================================= */
int main(void)
{
    /* ── Core HAL init ─────────────────────────────────────────────────────── */
    HAL_Init();
    SystemClock_Config();

    /* ── GPIO: Force-update button on PA0 (active LOW) ─────────────────────── */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin  = GPIO_PIN_0;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOA, &gpio);

    /* ── GPIO: Status LED on PC13 (active LOW) ──────────────────────────────── */
    __HAL_RCC_GPIOC_CLK_ENABLE();
    gpio.Pin   = GPIO_PIN_13;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &gpio);
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);   /* LED off */

    /* ── Decide: jump to app, or stay in update mode ────────────────────────── */
    uint8_t force_update = (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_RESET);

    if (!force_update && is_valid_app())
    {
        /* Blink LED once to show bootloader ran, then jump */
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
        HAL_Delay(80);
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
        HAL_Delay(20);
        jump_to_app();   /* Does not return */
    }

    /* ── Enter USB update mode ──────────────────────────────────────────────── */
    MX_USB_DEVICE_Init();

    send_str("\r\n================================================\r\n");
    send_str(" STM32F410RBT6 USB CDC Bootloader v1.0\r\n");
    send_str(" Flash: 128KB | SRAM: 32KB | Clock: 96MHz\r\n");
    send_str("================================================\r\n");
    send_str(" Commands: I=Info  E=Erase  W=Write  J=Jump\r\n");
    send_str("================================================\r\n");

    uint32_t boot_tick     = HAL_GetTick();
    uint8_t  host_seen     = 0;

    /* ── Main loop ───────────────────────────────────────────────────────────── */
    while (1)
    {
        /* Heartbeat LED: fast blink (100 ms period) while waiting */
        uint32_t now = HAL_GetTick();
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13,
            ((now % 200) < 50) ? GPIO_PIN_RESET : GPIO_PIN_SET);

        /* Auto-boot timeout: if host never connected */
        if (!host_seen && !update_mode &&
            ((HAL_GetTick() - boot_tick) > BOOT_TIMEOUT_MS))
        {
            if (is_valid_app())
            {
                send_str("[BOOT] Timeout – no host. Booting application.\r\n");
                HAL_Delay(50);
                jump_to_app();
            }
        }

        /* Service incoming USB data */
        if (rx_ready)
        {
            rx_ready   = 0;
            host_seen  = 1;
            process_command();
        }
    }
}

/* =============================================================================
 *  process_command()
 *
 *  Packet formats (all multi-byte integers are Big-Endian):
 *
 *  Host → 'I'                              → device info string
 *  Host → 'E'                              → erase app flash
 *  Host → 'W' <len_hi> <len_lo> <data[N]> <crc8_of(len_hi,len_lo,data)>
 *                                          → write chunk (N must be mult of 4)
 *  Host → 'J'  or  0x04 (EOT)             → jump to application
 * ============================================================================= */
static void process_command(void)
{
    if (usb_rx_len == 0) return;

    char     tmp[160];
    uint8_t  cmd = usb_rx_buf[0];

    switch (cmd)
    {
    /* ── INFO ─────────────────────────────────────────────────────────────── */
    case CMD_INFO:
        snprintf(tmp, sizeof(tmp),
            "[INFO] BL=0x%08lX  App=0x%08lX  AppValid=%s  WritePtr=0x%08lX\r\n",
            BOOTLOADER_BASE,
            APP_START_ADDR,
            is_valid_app() ? "YES" : "NO",
            write_ptr);
        send_str(tmp);
        break;

    /* ── ERASE ────────────────────────────────────────────────────────────── */
    case CMD_ERASE:
        send_str("[ERASE] Erasing sectors 1-4 (0x08004000-0x0801FFFF)...\r\n");
        if (flash_erase_app() == HAL_OK)
        {
            write_ptr   = APP_START_ADDR;
            update_mode = 1;
            send_byte(ACK);
            send_str("[ERASE] OK. Send 'W' chunks now.\r\n");
        }
        else
        {
            send_byte(NAK);
            send_str("[ERASE] FAILED!\r\n");
        }
        break;

    /* ── WRITE ────────────────────────────────────────────────────────────── */
    /*  Packet: 'W' + len_hi + len_lo + data[N] + crc8
     *  crc8 covers: len_hi, len_lo, data[0..N-1]                            */
    case CMD_WRITE:
    {
        if (!update_mode)
        {
            send_str("[WRITE] ERR: must erase first ('E').\r\n");
            send_byte(NAK);
            break;
        }
        if (usb_rx_len < 4)
        {
            send_str("[WRITE] ERR: packet too short.\r\n");
            send_byte(NAK);
            break;
        }

        uint16_t dlen = ((uint16_t)usb_rx_buf[1] << 8) | usb_rx_buf[2];

        /* Sanity: max chunk = 508 bytes (512 rx buf − 4 header bytes) */
        if (dlen == 0 || dlen > 508 || (dlen % 4) != 0)
        {
            snprintf(tmp, sizeof(tmp),
                "[WRITE] ERR: bad length %u (must be 4-508, multiple of 4).\r\n", dlen);
            send_str(tmp);
            send_byte(NAK);
            break;
        }

        uint16_t expected_pkt_len = 1u + 2u + dlen + 1u;   /* W + len + data + crc */
        if (usb_rx_len < expected_pkt_len)
        {
            send_str("[WRITE] ERR: incomplete packet.\r\n");
            send_byte(NAK);
            break;
        }

        /* CRC-8 covers len bytes + data bytes */
        uint8_t expected_crc = crc8_calc((const uint8_t*)&usb_rx_buf[1], 2u + dlen);
        uint8_t received_crc = usb_rx_buf[3 + dlen];

        if (expected_crc != received_crc)
        {
            snprintf(tmp, sizeof(tmp),
                "[WRITE] CRC mismatch: got 0x%02X expected 0x%02X\r\n",
                received_crc, expected_crc);
            send_str(tmp);
            send_byte(NAK);
            break;
        }

        /* Range check: don't write outside application region */
        if ((write_ptr < APP_START_ADDR) ||
            ((write_ptr + dlen) > (APP_START_ADDR + 112UL * 1024UL)))
        {
            send_str("[WRITE] ERR: write would exceed application region.\r\n");
            send_byte(NAK);
            break;
        }

        HAL_StatusTypeDef st = flash_write_chunk(write_ptr,
                                                  (const uint8_t*)&usb_rx_buf[3],
                                                  dlen);
        if (st == HAL_OK)
        {
            write_ptr += dlen;
            send_byte(ACK);
        }
        else
        {
            snprintf(tmp, sizeof(tmp),
                "[WRITE] Flash error at 0x%08lX\r\n", write_ptr);
            send_str(tmp);
            send_byte(NAK);
        }
        break;
    }

    /* ── JUMP ─────────────────────────────────────────────────────────────── */
    case CMD_JUMP:
    case EOT:
        if (is_valid_app())
        {
            send_str("[BOOT] Valid app found – jumping!\r\n");
            HAL_Delay(50);
            jump_to_app();
        }
        else
        {
            send_str("[BOOT] No valid application at 0x08004000.\r\n");
            send_byte(NAK);
        }
        break;

    default:
        /* Silently discard unknown single bytes (line endings, etc.) */
        break;
    }
}

/* =============================================================================
 *  Flash helpers
 * ============================================================================= */
static HAL_StatusTypeDef flash_erase_app(void)
{
    HAL_FLASH_Unlock();

    /* Clear all pending error flags before erase */
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP  | FLASH_FLAG_OPERR |
                           FLASH_FLAG_WRPERR | FLASH_FLAG_PGAERR |
                           FLASH_FLAG_PGSERR);

    FLASH_EraseInitTypeDef erase = {0};
    erase.TypeErase    = FLASH_TYPEERASE_SECTORS;
    erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;   /* 2.7 V – 3.6 V */
    erase.Sector       = APP_FIRST_SECTOR;
    erase.NbSectors    = APP_SECTOR_COUNT;

    uint32_t sector_err = 0;
    HAL_StatusTypeDef st = HAL_FLASHEx_Erase(&erase, &sector_err);

    HAL_FLASH_Lock();
    return st;
}

static HAL_StatusTypeDef flash_write_chunk(uint32_t       addr,
                                            const uint8_t *data,
                                            uint16_t       len)
{
    /* Caller guarantees len % 4 == 0 */
    HAL_FLASH_Unlock();
    HAL_StatusTypeDef st = HAL_OK;

    for (uint16_t i = 0; i < len; i += 4)
    {
        uint32_t word;
        memcpy(&word, data + i, 4);
        st = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr + i, word);
        if (st != HAL_OK) break;
    }

    HAL_FLASH_Lock();
    return st;
}

/* =============================================================================
 *  Application validity check
 *
 *  The first word at APP_START_ADDR is the initial stack pointer (IVT[0]).
 *  For a valid STM32F410 image it must:
 *    1. Not be 0xFFFFFFFF (erased flash)
 *    2. Point into the SRAM window (0x20000000 – 0x20008000)
 * ============================================================================= */
static int is_valid_app(void)
{
    uint32_t sp = *(volatile uint32_t*)APP_START_ADDR;
    return ( (sp != 0xFFFFFFFFUL) &&
             (sp >= SRAM_BASE_ADDR) &&
             (sp <= SRAM_END_ADDR) );
}

/* =============================================================================
 *  jump_to_app()
 *
 *  Clean shutdown of all peripherals, remap VTOR, restore MSP, branch.
 *  This function never returns.
 * ============================================================================= */
static void jump_to_app(void)
{
    typedef void (*AppResetHandler)(void);

    /* 1. Disable all interrupts */
    __disable_irq();

    /* 2. Deinitialise USB */
    USBD_DeInit(&hUsbDeviceFS);

    /* 3. Stop SysTick */
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL  = 0;

    /* 4. Disable and clear all NVIC pending IRQs
     *    STM32F410 has up to 8 NVIC registers (256 vectors)             */
    for (uint8_t i = 0; i < 8; i++)
    {
        NVIC->ICER[i] = 0xFFFFFFFFUL;   /* Disable  */
        NVIC->ICPR[i] = 0xFFFFFFFFUL;   /* Clear pending */
    }

    /* 5. Remap interrupt vector table to application */
    SCB->VTOR = APP_START_ADDR;

    /* 6. Load application stack pointer and jump */
    uint32_t       app_sp  = *(volatile uint32_t*)(APP_START_ADDR);
    uint32_t       app_pc  = *(volatile uint32_t*)(APP_START_ADDR + 4UL);
    AppResetHandler app    = (AppResetHandler)app_pc;

    __set_MSP(app_sp);
    __enable_irq();
    app();   /* ← execution transfers here */

    /* Should never reach here */
    while (1) { __NOP(); }
}

/* =============================================================================
 *  USB TX helpers
 * ============================================================================= */
static void send_byte(uint8_t b)
{
    uint32_t t = HAL_GetTick();
    while (CDC_Transmit_FS(&b, 1) == USBD_BUSY)
    {
        if ((HAL_GetTick() - t) > 100UL) break;
        HAL_Delay(1);
    }
}

static void send_str(const char *s)
{
    uint16_t len = (uint16_t)strlen(s);
    uint32_t t   = HAL_GetTick();
    while (CDC_Transmit_FS((uint8_t*)s, len) == USBD_BUSY)
    {
        if ((HAL_GetTick() - t) > 200UL) break;
        HAL_Delay(1);
    }
}

/* =============================================================================
 *  CRC-8 (simple XOR accumulator – matches Python host side)
 * ============================================================================= */
static uint8_t crc8_calc(const uint8_t *buf, uint16_t len)
{
    uint8_t crc = 0;
    for (uint16_t i = 0; i < len; i++) crc ^= buf[i];
    return crc;
}

/* =============================================================================
 *  CDC RX callback
 *  Called by usbd_cdc_if.c::CDC_Receive_FS().
 *  Add this to CDC_Receive_FS() in usbd_cdc_if.c:
 *      extern void Bootloader_CDC_RxCallback(uint8_t*, uint32_t);
 *      Bootloader_CDC_RxCallback(Buf, *Len);
 * ============================================================================= */
void Bootloader_CDC_RxCallback(uint8_t *buf, uint32_t len)
{
    if (len > sizeof(usb_rx_buf)) len = sizeof(usb_rx_buf);
    memcpy((void*)usb_rx_buf, buf, len);
    usb_rx_len = (uint16_t)len;
    rx_ready   = 1;
}

/* =============================================================================
 *  SystemClock_Config()
 *
 *  STM32F410RBT6 target: SYSCLK = 96 MHz, USB = 48 MHz
 *  Assumes 8 MHz HSE crystal.
 *
 *  PLL math (HSE = 8 MHz):
 *    VCO input  = HSE / PLLM = 8 / 8 = 1 MHz          (must be 1-2 MHz)
 *    VCO output = 1 × PLLN   = 1 × 192 = 192 MHz       (must be 100-432 MHz)
 *    SYSCLK     = VCO / PLLP = 192 / 2 = 96 MHz        (max 100 MHz for F410)
 *    USB clock  = VCO / PLLQ = 192 / 4 = 48 MHz  ✓     (must be exactly 48 MHz)
 *
 *  Flash latency: 3 WS required for 90 MHz < HCLK ≤ 100 MHz @ 3.3 V (RM0401 Table 6)
 * ============================================================================= */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    /* Enable voltage scaling for 96 MHz operation */
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    /* Configure HSE + PLL */
    osc.OscillatorType      = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState            = RCC_HSE_ON;
    osc.PLL.PLLState        = RCC_PLL_ON;
    osc.PLL.PLLSource       = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLM            = 8;               /* 8 MHz / 8  = 1 MHz VCO in  */
    osc.PLL.PLLN            = 192;             /* 1 MHz × 192 = 192 MHz VCO  */
    osc.PLL.PLLP            = RCC_PLLP_DIV2;  /* 192 / 2     = 96 MHz SYS   */
    osc.PLL.PLLQ            = 4;              /* 192 / 4     = 48 MHz USB   */

    if (HAL_RCC_OscConfig(&osc) != HAL_OK)
    {
        /* Clock failed – hang and let watchdog reset (if configured) */
        while (1) { __NOP(); }
    }

    /* Configure bus clocks */
    clk.ClockType      = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                         RCC_CLOCKTYPE_PCLK1  | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;    /* HCLK  = 96 MHz              */
    clk.APB1CLKDivider = RCC_HCLK_DIV2;      /* PCLK1 = 48 MHz (max 50 MHz) */
    clk.APB2CLKDivider = RCC_HCLK_DIV1;      /* PCLK2 = 96 MHz (max 100 MHz)*/

    /* 3 wait states needed for 90 < HCLK ≤ 100 MHz @ VDD 3.3 V */
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_3) != HAL_OK)
    {
        while (1) { __NOP(); }
    }
}
void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
}
