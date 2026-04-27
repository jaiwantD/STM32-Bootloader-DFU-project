/*
 * rcc_clock.h
 *
 *  Created on: 27-Apr-2026
 *      Author: ADMIN
 */

#ifndef DEVICE_DRIVERS_RCC_CLOCK_H_
#define DEVICE_DRIVERS_RCC_CLOCK_H_

#include "device_headers/stm32f446xx.h"



typedef struct
{
    volatile uint32_t *reg;
    uint32_t bit;
} CLK_t;

/* ================= AHB1 ================= */
#define CLK_GPIOA   (CLK_t){ &RCC->AHB1ENR, (1<<0) }
#define CLK_GPIOB   (CLK_t){ &RCC->AHB1ENR, (1<<1) }
#define CLK_GPIOC   (CLK_t){ &RCC->AHB1ENR, (1<<2) }
#define CLK_GPIOD   (CLK_t){ &RCC->AHB1ENR, (1<<3) }
#define CLK_GPIOE   (CLK_t){ &RCC->AHB1ENR, (1<<4) }
#define CLK_GPIOF   (CLK_t){ &RCC->AHB1ENR, (1<<5) }
#define CLK_GPIOG   (CLK_t){ &RCC->AHB1ENR, (1<<6) }
#define CLK_GPIOH   (CLK_t){ &RCC->AHB1ENR, (1<<7) }

#define CLK_CRC     (CLK_t){ &RCC->AHB1ENR, (1<<12) }
#define CLK_DMA1    (CLK_t){ &RCC->AHB1ENR, (1<<21) }
#define CLK_DMA2    (CLK_t){ &RCC->AHB1ENR, (1<<22) }

/* ================= AHB2 ================= */
#define CLK_OTGFS   (CLK_t){ &RCC->AHB2ENR, (1<<7) }
#define CLK_DCMI    (CLK_t){ &RCC->AHB2ENR, (1<<0) }

/* ================= AHB3 ================= */
#define CLK_FMC     (CLK_t){ &RCC->AHB3ENR, (1<<0) }
#define CLK_QSPI    (CLK_t){ &RCC->AHB3ENR, (1<<1) }

/* ================= APB1 ================= */
#define CLK_TIM2    (CLK_t){ &RCC->APB1ENR, (1<<0) }
#define CLK_TIM3    (CLK_t){ &RCC->APB1ENR, (1<<1) }
#define CLK_TIM4    (CLK_t){ &RCC->APB1ENR, (1<<2) }
#define CLK_TIM5    (CLK_t){ &RCC->APB1ENR, (1<<3) }
#define CLK_TIM6    (CLK_t){ &RCC->APB1ENR, (1<<4) }
#define CLK_TIM7    (CLK_t){ &RCC->APB1ENR, (1<<5) }
#define CLK_TIM12   (CLK_t){ &RCC->APB1ENR, (1<<6) }
#define CLK_TIM13   (CLK_t){ &RCC->APB1ENR, (1<<7) }
#define CLK_TIM14   (CLK_t){ &RCC->APB1ENR, (1<<8) }

#define CLK_WWDG    (CLK_t){ &RCC->APB1ENR, (1<<11) }

#define CLK_SPI2    (CLK_t){ &RCC->APB1ENR, (1<<14) }
#define CLK_SPI3    (CLK_t){ &RCC->APB1ENR, (1<<15) }

#define CLK_USART2  (CLK_t){ &RCC->APB1ENR, (1<<17) }
#define CLK_USART3  (CLK_t){ &RCC->APB1ENR, (1<<18) }
#define CLK_UART4   (CLK_t){ &RCC->APB1ENR, (1<<19) }
#define CLK_UART5   (CLK_t){ &RCC->APB1ENR, (1<<20) }

#define CLK_I2C1    (CLK_t){ &RCC->APB1ENR, (1<<21) }
#define CLK_I2C2    (CLK_t){ &RCC->APB1ENR, (1<<22) }
#define CLK_I2C3    (CLK_t){ &RCC->APB1ENR, (1<<23) }

#define CLK_CAN1    (CLK_t){ &RCC->APB1ENR, (1<<25) }
#define CLK_CAN2    (CLK_t){ &RCC->APB1ENR, (1<<26) }

#define CLK_PWR     (CLK_t){ &RCC->APB1ENR, (1<<28) }
#define CLK_DAC     (CLK_t){ &RCC->APB1ENR, (1<<29) }

/* ================= APB2 ================= */
#define CLK_TIM1    (CLK_t){ &RCC->APB2ENR, (1<<0) }
#define CLK_TIM8    (CLK_t){ &RCC->APB2ENR, (1<<1) }

#define CLK_USART1  (CLK_t){ &RCC->APB2ENR, (1<<4) }
#define CLK_USART6  (CLK_t){ &RCC->APB2ENR, (1<<5) }

#define CLK_ADC1    (CLK_t){ &RCC->APB2ENR, (1<<8) }
#define CLK_ADC2    (CLK_t){ &RCC->APB2ENR, (1<<9) }
#define CLK_ADC3    (CLK_t){ &RCC->APB2ENR, (1<<10) }

#define CLK_SDIO    (CLK_t){ &RCC->APB2ENR, (1<<11) }

#define CLK_SPI1    (CLK_t){ &RCC->APB2ENR, (1<<12) }
#define CLK_SPI4    (CLK_t){ &RCC->APB2ENR, (1<<13) }
#define CLK_SPI5    (CLK_t){ &RCC->APB2ENR, (1<<20) }

#define CLK_SYSCFG  (CLK_t){ &RCC->APB2ENR, (1<<14) }

void CLOCK_Enable(CLK_t clk);
void CLOCK_Disable(CLK_t clk);
#endif /* DEVICE_DRIVERS_RCC_CLOCK_H_ */
