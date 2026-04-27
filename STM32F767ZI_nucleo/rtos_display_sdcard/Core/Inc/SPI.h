/*
 * SPI.h
 *
 *  Created on: 16 Apr 2026
 *      Author: jelen
 */

#ifndef INC_SPI_H_
#define INC_SPI_H_

#include "stm32f7xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    SPI_HandleTypeDef *hspi;
    GPIO_TypeDef *cs_port;
    uint16_t cs_pin;
} Gen_SPI_info;
































#endif /* INC_SPI_H_ */
