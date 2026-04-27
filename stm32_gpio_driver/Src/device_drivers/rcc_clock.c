/*
 * rcc_clock.c
 *
 *  Created on: 27-Apr-2026
 *      Author: ADMIN
 */


#include "device_drivers/rcc_clock.h"


void CLOCK_Enable(CLK_t clk)
{
    *(clk.reg) |= clk.bit;
}


void CLOCK_Disable(CLK_t clk)
{
    *(clk.reg) &= ~(clk.bit);
}
