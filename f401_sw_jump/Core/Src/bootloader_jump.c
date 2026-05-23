/*
 * bootloader_jump.c
 *
 *  Created on: 21-May-2026
 *      Author: ADMIN
 */

#include "main.h"
#include "bootloader_jump.h"

#define SYS_MEM_ADDR 0x1FFF0000U

void JumpToBootloader(void)
{
    typedef void (*pFunction)(void);

    uint32_t JumpAddress;
    pFunction SysMemBootJump;

    __disable_irq();

    HAL_DeInit();

    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL  = 0;

    for(uint8_t i=0;i<8;i++)
    {
        NVIC->ICER[i]=0xFFFFFFFF;
        NVIC->ICPR[i]=0xFFFFFFFF;
    }

    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct={0};

    /* Release USB pins */
    GPIO_InitStruct.Pin = GPIO_PIN_11 | GPIO_PIN_12;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;

    HAL_GPIO_Init(GPIOA,&GPIO_InitStruct);

    HAL_RCC_DeInit();

    __DSB();
    __ISB();

    JumpAddress = *((volatile uint32_t*)(SYS_MEM_ADDR + 4U));

    SysMemBootJump = (pFunction)JumpAddress;

    __set_MSP(*((volatile uint32_t*)SYS_MEM_ADDR));

    SysMemBootJump();

    while(1)
    {
    }
}
