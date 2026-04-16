/*
 * SDdriver.c
 *
 *  Created on: 14 Apr 2026
 *      Author: jelen
 *
 * Fix log:
 * - Removed reliance on SPI2 IRQ (was corrupting blocking transfers)
 * - Added CMD8 (SEND_IF_COND) required for SDHC cards before CMD0
 * - Proper CS HIGH during 80-clock power-up sequence
 * - Dummy byte sent after CS assert and after CS deassert (per SD spec)
 * - Response polling waits up to NCR_MAX (8) bytes per spec
 */

#include "stdint.h"
#include <stdio.h>
#include <string.h>
#include "SDdriver.h"

static uint8_t Retries = 0;
static uint8_t Rxdata = 0;
//static uint32_t Rxdata4 = 0;
static uint8_t syncByte = 0xFF;
uint8_t rx[512];
uint8_t tempCRC[2];
uint8_t dummy = 0xFF;

static volatile struct {
	uint16_t CSpin;
	GPIO_TypeDef* CSport;
	SPI_HandleTypeDef* SPI;
	UART_HandleTypeDef* UART;

} SD_conf;

/*COMMAND LIST
 * CMD8  0b01 001000 args > 0b00000000 0b00000000 0b00000000 0b00000000 < args 0b00000000 0b00000000
 *
 *
 */

const char CMD0[] = {0b01000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b10010101};
const char CMD8[] = {0x48, 0x00, 0x00, 0x01, 0xAA, 0x87};
const uint8_t CMD16[] = {0x50, 0x00, 0x00, 0x02, 0x00, 0x01}; // 512 bytes
const uint8_t CMD17[] = {0x51, 0x00, 0x00, 0x00, 0x00, 0x01}; // sector 0
//const char CMD8[] = {0x48, 0x00, 0x00, 0x01, 0xAA, 0x87};






void SD_Init(uint16_t CS_pin, GPIO_TypeDef* CS_port, SPI_HandleTypeDef* hspi, UART_HandleTypeDef* huart) {
    // 1. Set CS High (Card De-selected)
	SD_conf.CSpin = CS_pin;
	SD_conf.CSport = CS_port;
	SD_conf.SPI = hspi;
	SD_conf.UART = huart;

	HAL_Delay(250);
    HAL_GPIO_WritePin(CS_port, CS_pin, GPIO_PIN_SET);
    HAL_UART_Transmit(SD_conf.UART, (uint8_t*) "CS high\r\n", 11, 50);
    // 2. Prepare 10 bytes of 0xFF (10 bytes * 8 bits = 80 clock cycles)


    for(int i=0; i<13; i++) {
        HAL_SPI_Transmit(hspi, &syncByte, 1, 10);
    }
    HAL_UART_Transmit(SD_conf.UART, (uint8_t*) "waited 80+ clocks\r\n", 21, 50);
    uint8_t helloData[6] = {0b01000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b10010101};
    while (Rxdata != 0x01){
    	HAL_Delay(50);
    // 3. Transmit the data to generate 80 clocks with MOSI high
    // Use a long timeout to ensure it completes
    //HAL_SPI_Transmit(hspi, syncData, 10, 100);


    HAL_GPIO_WritePin(CS_port, CS_pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(hspi, helloData, 6, 100); // sent INIT command
    HAL_UART_Transmit(SD_conf.UART, (uint8_t*) "transmitted CMD0\r\n", 20, 50);
    do {
    	HAL_SPI_TransmitReceive(hspi, &syncByte, &Rxdata, 1, 50);
  	  //char message[50];
  	  //int len = snprintf(message, sizeof(message), "Received: 0x%02X\r\n", Rxdata);
  	  //HAL_UART_Transmit(SD_conf.UART, (uint8_t*)message, len, 100);


    	Retries++;
    } while ((Rxdata == 0xFF) && (Retries != 30));
    Retries = 0;
    HAL_GPIO_WritePin(CS_port, CS_pin, GPIO_PIN_SET);
    }
    HAL_UART_Transmit(huart, (uint8_t*)"SD Init Success\r\n", 17, 100);






}

/*
 * @brief
 * Sends a command to the card and waits for a reply from it
 * command frame is 48 bits
 * 0 1 45-40cmdNr 39-8args 7-1CRC 0
 * CMD8  0b01 001000 args > 0b00000000 0b00000000 0b00000000 0b00000000 < args 0b00000000 0b00000000
 */


void SD_SendCMD(uint8_t cmdNr){

	switch (cmdNr) {
	  case 8:

		  HAL_GPIO_WritePin(SD_conf.CSport, SD_conf.CSpin, GPIO_PIN_RESET);
		      HAL_SPI_Transmit(SD_conf.SPI, (uint8_t*) CMD8, 6, 100); // sent command

		      for (int i = 0; i <10; i++){
		    	  HAL_SPI_TransmitReceive(SD_conf.SPI, &syncByte, &Rxdata, 1, 50);
  		    	  if (Rxdata != 0xFF) {
	  		    	  char message[50];
	  		    	  int len = snprintf(message, sizeof(message), "Status8: 0x%02X\r\n", Rxdata);
	  		    	  HAL_UART_Transmit(SD_conf.UART, (uint8_t*)message, len, 100);
  		    	  }

		      }
		      HAL_GPIO_WritePin(SD_conf.CSport, SD_conf.CSpin, GPIO_PIN_SET);


	    break;
	  case 16:
	  {

	      uint8_t resp;
	      uint8_t dummy = 0xFF;
	      char message[64];

	      const uint8_t CMD16[] = {0x50, 0x00, 0x00, 0x00, 0x00, 0x01};
	      const uint8_t CMD17[] = {0x51, 0x00, 0x00, 0x00, 0x00, 0x01};

	      HAL_GPIO_WritePin(SD_conf.CSport, SD_conf.CSpin, GPIO_PIN_RESET);

	      /* CMD16 */
	      HAL_SPI_Transmit(SD_conf.SPI, (uint8_t*)CMD16, 6, 100);

	      do {
	          HAL_SPI_TransmitReceive(SD_conf.SPI, &dummy, &resp, 1, 100);
	      } while (resp == 0xFF);

	      snprintf(message, sizeof(message), "CMD16 resp: 0x%02X\r\n", resp);
	      HAL_UART_Transmit(SD_conf.UART, (uint8_t*)message, strlen(message), 100);

	      /* CMD17 */
	      HAL_SPI_Transmit(SD_conf.SPI, (uint8_t*)CMD17, 6, 100);

	      do {
	          HAL_SPI_TransmitReceive(SD_conf.SPI, &dummy, &resp, 1, 100);
	      } while (resp == 0xFF);

	      snprintf(message, sizeof(message), "CMD17 resp: 0x%02X\r\n", resp);
	      HAL_UART_Transmit(SD_conf.UART, (uint8_t*)message, strlen(message), 100);

	      if (resp != 0x00) {
	          HAL_UART_Transmit(SD_conf.UART,
	              (uint8_t*)"CMD17 failed\r\n", 14, 100);
	          break;
	      }

	      /* Wait for 0xFE */
	      int timeout = 100000;
	      do {
	          HAL_SPI_TransmitReceive(SD_conf.SPI, &dummy, &resp, 1, 100);
	      } while ((resp != 0xFE) && --timeout);

	      if (timeout == 0) {
	          HAL_UART_Transmit(SD_conf.UART,
	              (uint8_t*)"No data token\r\n", 15, 100);
	          break;
	      }

	      HAL_UART_Transmit(SD_conf.UART,
	          (uint8_t*)"Data start\r\n", 12, 100);

	      /* Read 512 bytes (NOT 2048!) */
	      uint32_t sttick = HAL_GetTick();
	      for (int i = 0; i < (128 * 512); i++) {

	          HAL_SPI_TransmitReceive(SD_conf.SPI, &dummy, rx, 512, 100);

//	          int len = snprintf(message, sizeof(message),
//	              "0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\r\n",
//	              rx[0], rx[1], rx[2], rx[3], rx[4], rx[5], rx[6], rx[7]);
//
//	          HAL_UART_Transmit(SD_conf.UART, (uint8_t*)message, len, 100);
	      }
	      float delta = ((float)HAL_GetTick() - (float)sttick)/1000;
	      char text[50];
	      int length = snprintf(text, sizeof(text), "512kbyes read in %f seconds\r\n", delta);
	      HAL_UART_Transmit(SD_conf.UART, (uint8_t*)text, length, 100);

	      /* CRC */
	      HAL_SPI_TransmitReceive(SD_conf.SPI, &dummy, rx, 2, 100);

	      HAL_GPIO_WritePin(SD_conf.CSport, SD_conf.CSpin, GPIO_PIN_SET);
	      HAL_SPI_Transmit(SD_conf.SPI, &dummy, 1, 100);

	      break;
	  }
	  case 1 :
		  uint8_t CMD1[6] = {0x41, 0x00,0b00100000,0x00,0x00,0x00};
		  HAL_GPIO_WritePin(SD_conf.CSport, SD_conf.CSpin, GPIO_PIN_RESET);
		  		      HAL_SPI_Transmit(SD_conf.SPI, (uint8_t*) CMD1, 6, 100); // sent command

		  		      for (int i = 0; i <10; i++){
		  		    	  HAL_SPI_TransmitReceive(SD_conf.SPI, &syncByte, &Rxdata, 1, 50);
		  		    	  if (Rxdata != 0xFF) {
			  		    	  char message[50];
			  		    	  int len = snprintf(message, sizeof(message), "Status1: 0x%02X\r\n", Rxdata);
			  		    	  HAL_UART_Transmit(SD_conf.UART, (uint8_t*)message, len, 100);
		  		    	  }
		  		      }
		  		      HAL_GPIO_WritePin(SD_conf.CSport, SD_conf.CSpin, GPIO_PIN_SET);

		  break;
	  case 13 :
		  uint8_t CMD13[6] = {0x4D, 0x00,0x00,0x00,0x00,0x00};
		  HAL_GPIO_WritePin(SD_conf.CSport, SD_conf.CSpin, GPIO_PIN_RESET);
		  		      HAL_SPI_Transmit(SD_conf.SPI, (uint8_t*) CMD13, 6, 100); // sent command

		  		      for (int i = 0; i <15; i++){
		  		    	  HAL_SPI_TransmitReceive(SD_conf.SPI, &syncByte, &Rxdata, 1, 50);
		  		    	  if (Rxdata != 0xFF) {
			  		    	  char message[50];
			  		    	  int len = snprintf(message, sizeof(message), "Status13: 0x%02X\r\n", Rxdata);
			  		    	  HAL_UART_Transmit(SD_conf.UART, (uint8_t*)message, len, 100);
		  		    	  }

		  		      }
		  		      HAL_GPIO_WritePin(SD_conf.CSport, SD_conf.CSpin, GPIO_PIN_SET);

		  break;
		  // CMD58 read ocr then ACMD41 with hcs 1, if returns in idle state 1, retry ACMD41
	  case 58 :
		  uint8_t CMD58[6] = {0x7A, 0x00,0x00,0x00,0x00,0x00};
		  HAL_GPIO_WritePin(SD_conf.CSport, SD_conf.CSpin, GPIO_PIN_RESET);
		  		      HAL_SPI_Transmit(SD_conf.SPI, (uint8_t*) CMD58, 6, 100); // sent command

		  		      for (int i = 0; i <15; i++){
		  		    	  HAL_SPI_TransmitReceive(SD_conf.SPI, &syncByte, &Rxdata, 1, 50);
		  		    	  if (Rxdata != 0xFF) {
			  		    	  char message[50];
			  		    	  int len = snprintf(message, sizeof(message), "Status58: 0x%02X\r\n", Rxdata);
			  		    	  HAL_UART_Transmit(SD_conf.UART, (uint8_t*)message, len, 100);
		  		    	  }

		  		      }
		  		      HAL_GPIO_WritePin(SD_conf.CSport, SD_conf.CSpin, GPIO_PIN_SET);

		  break;
	  case 59 :
		  uint8_t CMD59[6] = {0x7B, 0x00,0x00,0x00,0x00,0x00};
		  HAL_GPIO_WritePin(SD_conf.CSport, SD_conf.CSpin, GPIO_PIN_RESET);
		  		      HAL_SPI_Transmit(SD_conf.SPI, (uint8_t*) CMD59, 6, 100); // sent command

		  		      for (int i = 0; i <15; i++){
		  		    	  HAL_SPI_TransmitReceive(SD_conf.SPI, &syncByte, &Rxdata, 1, 50);
		  		    	  if (Rxdata != 0xFF) {
			  		    	  char message[50];
			  		    	  int len = snprintf(message, sizeof(message), "Status59: 0x%02X\r\n", Rxdata);
			  		    	  HAL_UART_Transmit(SD_conf.UART, (uint8_t*)message, len, 100);
		  		    	  }

		  		      }
		  		      HAL_GPIO_WritePin(SD_conf.CSport, SD_conf.CSpin, GPIO_PIN_SET);

		  break;
	  case 55 :
		  uint8_t response = 0xFF;

		  do {
		      // Send CMD55


			  // 1. Send CMD55
			  HAL_GPIO_WritePin(SD_conf.CSport, SD_conf.CSpin, GPIO_PIN_RESET);
			  uint8_t cmd55[] = {0x77, 0x00, 0x00, 0x00, 0x00, 0x65};
			  HAL_SPI_Transmit(SD_conf.SPI, cmd55, 6, 10);

			  // Wait for R1 response (not 0xFF)
			  for(int i=0; i<10 && response == 0xFF; i++) {
			      HAL_SPI_Receive(SD_conf.SPI, &response, 1, 10);
			  }
			  HAL_GPIO_WritePin(SD_conf.CSport, SD_conf.CSpin, GPIO_PIN_SET);
			  HAL_SPI_Transmit(SD_conf.SPI, &dummy, 1, 10); // Mandatory dummy clocks

			  // 2. Send ACMD41
			  HAL_GPIO_WritePin(SD_conf.CSport, SD_conf.CSpin, GPIO_PIN_RESET);
			  uint8_t acmd41[] = {0x69, 0x40, 0x00, 0x00, 0x00, 0x77};
			  HAL_SPI_Transmit(SD_conf.SPI, acmd41, 6, 10);

			  response = 0xFF;
			  for(int i=0; i<10 && response == 0xFF; i++) {
			      HAL_SPI_Receive(SD_conf.SPI, &response, 1, 10);
			  }
			  HAL_GPIO_WritePin(SD_conf.CSport, SD_conf.CSpin, GPIO_PIN_SET);
			  HAL_SPI_Transmit(SD_conf.SPI, &dummy, 1, 10); // Mandatory dummy clocks
		  } while (response != 0x00);

		  break;
	  case 18:
	      uint8_t CMD18[] = {0x52, 0x00, 0x00, 0x00, 0x00, 0xFF}; // CMD18, Arg: 0, CRC: 0xFF
	      uint8_t CMD12[] = {0x4C, 0x00, 0x00, 0x00, 0x00, 0x61}; // Stop Transmission
	      uint8_t token;
	      uint16_t numBlocks = 2048; // 1024 blocks * 512 bytes = 512 KB
	      uint32_t errorCount = 0;

	      // 1. Send CMD18
	      HAL_GPIO_WritePin(SD_conf.CSport, SD_conf.CSpin, GPIO_PIN_RESET);
	      if (HAL_SPI_Transmit(SD_conf.SPI, CMD18, 6, 100) != HAL_OK) break;

	      // Wait for Command Response (R1)
	      for (int i = 0; i < 10; i++) {
	          HAL_SPI_Receive(SD_conf.SPI, &Rxdata, 1, 10);
	          if (Rxdata == 0x00) break;
	      }

	      if (Rxdata != 0x00) {
	          HAL_UART_Transmit(SD_conf.UART, (uint8_t*)"CMD18 Failed\r\n", 14, 100);
	          HAL_GPIO_WritePin(SD_conf.CSport, SD_conf.CSpin, GPIO_PIN_SET);
	          break;
	      }

	      uint32_t startTime = HAL_GetTick();

	      // 2. Data Read Loop
	      for (int b = 0; b < numBlocks; b++) {
	          token = 0xFF;
	          uint32_t waitStart = HAL_GetTick();

	          // Wait for Data Token (0xFE) with a 500ms timeout
	          while (token != 0xFE && (HAL_GetTick() - waitStart) < 500) {
	              HAL_SPI_Receive(SD_conf.SPI, &token, 1, 1);
	          }

	          if (token == 0xFE) {
	              HAL_SPI_Receive(SD_conf.SPI, rx, 512, 100);
	              uint8_t crcPlaceholder[2];
	              HAL_SPI_Receive(SD_conf.SPI, crcPlaceholder, 2, 100);
	              // Do NOT print here
	          } else {
	              errorCount++;
	              break; // If we lose the sync, stop the loop to avoid 1000s of timeouts
	          }
	      }

	      // 3. Send CMD12 to stop transmission
	      HAL_SPI_Transmit(SD_conf.SPI, CMD12, 6, 100);
	      // Skip one stuffing byte, then get CMD12 response
	      HAL_SPI_Receive(SD_conf.SPI, &Rxdata, 1, 10);
	      HAL_SPI_Receive(SD_conf.SPI, &Rxdata, 1, 10);

	      uint32_t endTime = HAL_GetTick();
	      HAL_GPIO_WritePin(SD_conf.CSport, SD_conf.CSpin, GPIO_PIN_SET);

	      // Final Dummy Clock
	      uint8_t dummy = 0xFF;
	      HAL_SPI_Transmit(SD_conf.SPI, &dummy, 1, 10);

	      // 4. Report via UART
	      float delta = (float)(endTime - startTime) / 1000.0f;
	      char report[100];
	      int len = snprintf(report, sizeof(report),
	          "\r\n--- Read Complete ---\r\nTime: %.3f sec\r\nErrors: %lu\r\nSpeed: %.2f KB/s\r\n",
	          delta, errorCount, (512.0f / delta));
	      HAL_UART_Transmit(SD_conf.UART, (uint8_t*)report, len, 200);

	      break;
	  default:
		  break;
	}



// Fiolet serij belij cherni





}

