/*
 * SDdriver.h
 *
 *  Created on: 14 Apr 2026
 *      Author: jelen
 */


#ifndef CUSTOMDRIVERS_INCLUDES_SDDRIVER_H_
#define CUSTOMDRIVERS_INCLUDES_SDDRIVER_H_

#include "stm32g0xx_hal.h"


void SD_Init(uint16_t CS_pin, GPIO_TypeDef* CS_port, SPI_HandleTypeDef* hspi, UART_HandleTypeDef* huart);
void SD_SendCMD(uint8_t cmdNr);



#endif /* CUSTOMDRIVERS_INCLUDES_SDDRIVER_H_ */
