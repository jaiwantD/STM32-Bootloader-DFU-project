/*
 * gpio.h
 *
 *  Created on: 27-Apr-2026
 *      Author: JAIWANT D
 */

#ifndef DEVICE_DRIVERS_GPIO_H_
#define DEVICE_DRIVERS_GPIO_H_

typedef struct{
	GPIO_TypeDef *port;
	uint8_t pin;
	uint8_t mode;

    uint8_t otype;
    uint8_t speed;
    uint8_t pupd;
    uint8_t alt;
}GPIO_Config;

#define input_mode     0
#define output_mode    1
#define altf_mode     2
#define analog_mode    3




#endif /* DEVICE_DRIVERS_GPIO_H_ */
