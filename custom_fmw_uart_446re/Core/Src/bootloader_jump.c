/*
 * bootloader_jump.c
 *
 *  Created on: 25-Apr-2026
 *      Author: ADMIN
 */

#include "stm32f4xx_hal.h"

#define SYS_MEM_ADDR 0x1FFF0000U   // System Memory base for STM32F446
void JumpToBootloader(void)
{
    void (*SysMemBootJump)(void);
    uint32_t addr = SYS_MEM_ADDR;
    /* 1. Disable interrupts */
    __disable_irq();
    HAL_DeInit();
    HAL_RCC_DeInit();

    /* 3. Stop SysTick */
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL  = 0;

    /* 4. Disable & clear all NVIC interrupts */
    for (uint32_t i = 0; i < 8; i++) {
        NVIC->ICER[i] = 0xFFFFFFFF;
        NVIC->ICPR[i] = 0xFFFFFFFF;
    }

    for (volatile uint32_t i = 0; i < 100000; i++);
    /* 7. Get reset handler from system memory */
    SysMemBootJump = (void (*)(void)) (*((uint32_t *)(addr + 4U)));

    /* 8. Set MSP to system memory value */
    __set_MSP(*(uint32_t *)addr);
    /* 9. Jump to system bootloader */
    SysMemBootJump();

    /* Should never return */
    while (1) { }
}
