#ifndef W5500_H_
#define W5500_H_

#include "main.h"   /* brings in HAL types (SPI_HandleTypeDef, GPIO, etc.) */
#include <stdint.h>

/* ------------------------------------------------------------------ */
/*  Pin mapping — EDIT these to match your actual pinout                */
/* ------------------------------------------------------------------ */
#define W5500_CS_PORT     GPIOB
#define W5500_CS_PIN      GPIO_PIN_12
#define W5500_RST_PORT    GPIOC
#define W5500_RST_PIN     GPIO_PIN_5

/* ------------------------------------------------------------------ */
/*  Status snapshot returned by W5500_GetStatus()                       */
/* ------------------------------------------------------------------ */
typedef struct
{
    uint8_t mac[6];
    uint8_t ip[4];
    uint8_t gw[4];
    uint8_t sn[4];
    uint8_t link_up;      /* 1 = link up, 0 = link down          */
    uint8_t speed_100m;   /* 1 = 100 Mbps, 0 = 10 Mbps            */
    uint8_t full_duplex;  /* 1 = full duplex, 0 = half duplex     */
    uint8_t chip_version; /* should read 0x04 on a genuine W5500  */
} W5500_Status_t;

/* ------------------------------------------------------------------ */
/*  API                                                                  */
/* ------------------------------------------------------------------ */

/* Call once at startup. hspi must already be initialized (MX_SPIx_Init())
 * with 8-bit data size, Mode 0 (CPOL=0, CPHA=0), NSS = Soft.
 * huart_dbg is used for human-readable status prints (may be NULL to
 * disable debug prints entirely). Returns 1 on success (chip detected
 * and responding), 0 on failure (e.g. VERSIONR readback mismatch). */
uint8_t W5500_Init(SPI_HandleTypeDef *hspi, UART_HandleTypeDef *huart_dbg);

/* Hardware-resets the chip via W5500_RST_PIN (blocking, ~5 ms total). */
void W5500_Reset(void);

/* Network configuration setters/getters */
void W5500_SetMAC(const uint8_t mac[6]);
void W5500_SetIP(const uint8_t ip[4], const uint8_t subnet[4], const uint8_t gateway[4]);
void W5500_GetMAC(uint8_t mac[6]);
void W5500_GetIP(uint8_t ip[4]);

/* Quick link check: returns 1 if the PHY reports link up, else 0 */
uint8_t W5500_IsLinkUp(void);

/* Fills status with a full live snapshot (MAC, IP, link/speed/duplex, chip version) */
void W5500_GetStatus(W5500_Status_t *status);

/* Prints a human-readable status block over the debug UART passed to W5500_Init() */
void W5500_PrintStatus(void);

#endif /* W5500_H_ */
