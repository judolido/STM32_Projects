/**
 * @file sd_card_spi.h
 * @brief SD Card driver for SPI interface
 */

#ifndef SD_CARD_SPI_H
#define SD_CARD_SPI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f7xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

/* SD Card Commands */
#define CMD0    (0)         /* GO_IDLE_STATE */
#define CMD1    (1)         /* SEND_OP_COND (MMC) */
#define ACMD41  (0x80+41)   /* SEND_OP_COND (SDC) */
#define CMD8    (8)         /* SEND_IF_COND */
#define CMD9    (9)         /* SEND_CSD */
#define CMD10   (10)        /* SEND_CID */
#define CMD12   (12)        /* STOP_TRANSMISSION */
#define CMD16   (16)        /* SET_BLOCKLEN */
#define CMD17   (17)        /* READ_SINGLE_BLOCK */
#define CMD18   (18)        /* READ_MULTIPLE_BLOCK */
#define CMD23   (23)        /* SET_BLOCK_COUNT (MMC) */
#define ACMD23  (0x80+23)   /* SET_WR_BLK_ERASE_COUNT (SDC) */
#define CMD24   (24)        /* WRITE_BLOCK */
#define CMD25   (25)        /* WRITE_MULTIPLE_BLOCK */
#define CMD32   (32)        /* ERASE_ER_BLK_START */
#define CMD33   (33)        /* ERASE_ER_BLK_END */
#define CMD38   (38)        /* ERASE */
#define CMD55   (55)        /* APP_CMD */
#define CMD58   (58)        /* READ_OCR */

/* Card type flags (CardType) */
#define CT_MMC      0x01    /* MMC ver 3 */
#define CT_SD1      0x02    /* SD ver 1 */
#define CT_SD2      0x04    /* SD ver 2 */
#define CT_SDC      (CT_SD1|CT_SD2) /* SD */
#define CT_BLOCK    0x08    /* Block addressing */

typedef enum {
    SD_OK = 0,
    SD_ERROR = 1,
    SD_TIMEOUT = 2,
    SD_NOT_READY = 3,
    SD_INVALID_PARAMETER = 4
} SD_StatusTypeDef;

typedef struct {
    SPI_HandleTypeDef *hspi;
    GPIO_TypeDef *cs_port;
    uint16_t cs_pin;
    uint8_t card_type;
    bool initialized;
} SD_CardInfo;

/* Public functions */
SD_StatusTypeDef SD_Init(SD_CardInfo *card);
SD_StatusTypeDef SD_ReadSingleBlock(SD_CardInfo *card, uint32_t sector, uint8_t *buffer);
SD_StatusTypeDef SD_ReadMultipleBlocks(SD_CardInfo *card, uint32_t sector, uint8_t *buffer, uint32_t count);
SD_StatusTypeDef SD_WriteSingleBlock(SD_CardInfo *card, uint32_t sector, const uint8_t *buffer);
SD_StatusTypeDef SD_WriteMultipleBlocks(SD_CardInfo *card, uint32_t sector, const uint8_t *buffer, uint32_t count);
SD_StatusTypeDef SD_GetCardInfo(SD_CardInfo *card, uint32_t *sectors);
bool SD_IsInitialized(SD_CardInfo *card);

#ifdef __cplusplus
}
#endif

#endif /* SD_CARD_SPI_H */
