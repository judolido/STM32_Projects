/**
 * @file sd_card_spi.c
 * @brief SD Card driver for SPI interface (blocking mode, FreeRTOS compatible)
 *
 * IMPORTANT HARDWARE NOTE:
 *   SD cards require SPI Mode 0 (CPOL=0, CPHA=0).
 *   SPI1 must be configured with CLKPolarity=LOW and CLKPhase=1EDGE.
 *   SPI1 must NOT have DMA linked - this driver uses blocking calls only.
 */

#include "sd_card_spi.h"
#include "cmsis_os.h"
#include <string.h>

#define SD_TIMEOUT      500     /* Timeout in ms */
#define SD_BLOCK_SIZE   512     /* SD card block size */

/* Internal function prototypes */
static void SD_CS_Low(SD_CardInfo *card);
static void SD_CS_High(SD_CardInfo *card);
static uint8_t SD_SendCommand(SD_CardInfo *card, uint8_t cmd, uint32_t arg);
static bool SD_WaitReady(SD_CardInfo *card, uint32_t timeout_ms);
static uint8_t SD_SPI_TransferByte(SD_CardInfo *card, uint8_t data);
static void SD_SPI_ReadBytes(SD_CardInfo *card, uint8_t *buffer, uint16_t len);
static void SD_SPI_WriteBytes(SD_CardInfo *card, const uint8_t *buffer, uint16_t len);

/**
 * @brief Select SD card (CS low) with a dummy byte before and after
 */
static void SD_CS_Low(SD_CardInfo *card) {
    HAL_GPIO_WritePin(card->cs_port, card->cs_pin, GPIO_PIN_SET);
    SD_SPI_TransferByte(card, 0xFF);  /* Dummy byte before CS */
    HAL_GPIO_WritePin(card->cs_port, card->cs_pin, GPIO_PIN_RESET);
    SD_SPI_TransferByte(card, 0xFF);  /* Dummy byte after CS */
}

/**
 * @brief Deselect SD card (CS high) with a trailing dummy byte
 */
static void SD_CS_High(SD_CardInfo *card) {
    HAL_GPIO_WritePin(card->cs_port, card->cs_pin, GPIO_PIN_SET);
    SD_SPI_TransferByte(card, 0xFF);  /* Release bus */
}

/**
 * @brief Transfer single byte via SPI (blocking)
 */
static uint8_t SD_SPI_TransferByte(SD_CardInfo *card, uint8_t data) {
    uint8_t rx_data = 0xFF;
    HAL_SPI_TransmitReceive(card->hspi, &data, &rx_data, 1, HAL_MAX_DELAY);
    return rx_data;
}

/**
 * @brief Read multiple bytes via SPI
 */
static void SD_SPI_ReadBytes(SD_CardInfo *card, uint8_t *buffer, uint16_t len) {
    /* Use TransmitReceive so MOSI stays high (0xFF) while reading */
    uint8_t dummy = 0xFF;
    for (uint16_t i = 0; i < len; i++) {
        HAL_SPI_TransmitReceive(card->hspi, &dummy, &buffer[i], 1, HAL_MAX_DELAY);
    }
}

/**
 * @brief Write multiple bytes via SPI
 */
static void SD_SPI_WriteBytes(SD_CardInfo *card, const uint8_t *buffer, uint16_t len) {
    uint8_t rx;
    for (uint16_t i = 0; i < len; i++) {
        HAL_SPI_TransmitReceive(card->hspi, (uint8_t*)&buffer[i], &rx, 1, HAL_MAX_DELAY);
    }
}

/**
 * @brief Wait until SD card releases MISO (ready = 0xFF)
 */
static bool SD_WaitReady(SD_CardInfo *card, uint32_t timeout_ms) {
    uint32_t start = HAL_GetTick();
    uint8_t response;

    do {
        response = SD_SPI_TransferByte(card, 0xFF);
        if (response == 0xFF) {
            return true;
        }
        osDelay(1);
    } while ((HAL_GetTick() - start) < timeout_ms);

    return false;
}

/**
 * @brief Send SD command and return R1 response byte
 *
 * For ACMD (cmd | 0x80), sends CMD55 prefix first.
 * CS is asserted here and left asserted for caller to read additional response bytes.
 * Caller must call SD_CS_High() when done.
 */
static uint8_t SD_SendCommand(SD_CardInfo *card, uint8_t cmd, uint32_t arg) {
    uint8_t response;
    uint8_t retry;

    /* ACMD: send CMD55 first, then the actual command */
    if (cmd & 0x80) {
        cmd &= 0x7F;
        response = SD_SendCommand(card, CMD55, 0);
        if (response > 1) return response;
        SD_CS_High(card);
    }

    /* Assert CS */
    SD_CS_Low(card);

    /* Build and send 6-byte command packet */
    uint8_t packet[6];
    packet[0] = 0x40 | cmd;
    packet[1] = (uint8_t)(arg >> 24);
    packet[2] = (uint8_t)(arg >> 16);
    packet[3] = (uint8_t)(arg >> 8);
    packet[4] = (uint8_t)(arg);

    /* CRC: only CMD0 and CMD8 need valid CRC in SPI mode */
    if (cmd == CMD0) packet[5] = 0x95;
    else if (cmd == CMD8) packet[5] = 0x87;
    else packet[5] = 0x01;  /* Dummy CRC (stop bit only) */

    SD_SPI_WriteBytes(card, packet, 6);

    /* Skip stuff byte for CMD12 (stop transmission) */
    if (cmd == CMD12) {
        SD_SPI_TransferByte(card, 0xFF);
    }

    /* Wait for response: bit7 = 0 means valid R1, timeout after 10 tries */
    retry = 10;
    do {
        response = SD_SPI_TransferByte(card, 0xFF);
    } while ((response & 0x80) && --retry);

    return response;
}

/**
 * @brief Initialize SD card
 *
 * Call this once. The driver will:
 *   1. Send 80+ dummy clocks with CS HIGH (power-up sequence)
 *   2. Send CMD0 to enter SPI mode (expect R1 = 0x01)
 *   3. Send CMD8 to determine card version
 *   4. Send ACMD41 to initialize
 *   5. Check CCS bit via CMD58 to determine SDHC vs SDSC
 *   6. Increase SPI clock speed for data transfer
 */
SD_StatusTypeDef SD_Init(SD_CardInfo *card) {
    uint8_t response;
    uint8_t ocr[4];
    uint32_t timeout;

    if (!card || !card->hspi) {
        return SD_INVALID_PARAMETER;
    }

    card->initialized = false;
    card->card_type = 0;

    /* Step 1: Power-up: CS HIGH, send >=74 dummy clocks */
    SD_CS_High(card);
    osDelay(100);  /* Wait for card power stabilization */

    for (uint8_t i = 0; i < 20; i++) {  /* 20 bytes = 160 clocks */
        SD_SPI_TransferByte(card, 0xFF);
    }
    osDelay(10);

    /* Step 2: CMD0 - Enter SPI mode. Expect 0x01 (in idle state). */
    timeout = 20;
    do {
        response = SD_SendCommand(card, CMD0, 0);
        SD_CS_High(card);
        if (response == 0x01) break;
        osDelay(50);
    } while (--timeout);

    if (response != 0x01) {
        return SD_ERROR;  /* Card not responding */
    }

    /* Step 3: CMD8 - Check voltage range, determine SDv1 vs SDv2 */
    response = SD_SendCommand(card, CMD8, 0x1AA);  /* VHS=1 (2.7-3.6V), check=0xAA */

    if (response == 0x01) {
        /* SDv2 or later - read 4-byte R7 response */
        for (uint8_t i = 0; i < 4; i++) {
            ocr[i] = SD_SPI_TransferByte(card, 0xFF);
        }
        SD_CS_High(card);

        if (ocr[2] == 0x01 && ocr[3] == 0xAA) {
            /* Voltage OK, initialize with ACMD41 (HCS=1 to support SDHC) */
            timeout = 1000;
            do {
                response = SD_SendCommand(card, ACMD41, 0x40000000UL);  /* HCS bit */
                SD_CS_High(card);
                if (response == 0x00) break;
                osDelay(1);
            } while (--timeout);

            if (timeout == 0 || response != 0x00) {
                return SD_ERROR;  /* Initialization timeout */
            }

            /* CMD58: Read OCR to check CCS (Card Capacity Status) bit */
            response = SD_SendCommand(card, CMD58, 0);
            if (response == 0x00) {
                for (uint8_t i = 0; i < 4; i++) {
                    ocr[i] = SD_SPI_TransferByte(card, 0xFF);
                }
                /* CCS bit (OCR[0] bit 6) = 1 means SDHC/SDXC (block addressing) */
                card->card_type = (ocr[0] & 0x40) ? (CT_SD2 | CT_BLOCK) : CT_SD2;
            }
            SD_CS_High(card);
        } else {
            SD_CS_High(card);
            return SD_ERROR;  /* Voltage mismatch */
        }
    } else {
        /* SDv1 or MMC */
        SD_CS_High(card);

        response = SD_SendCommand(card, ACMD41, 0);
        SD_CS_High(card);

        if (response <= 1) {
            /* SDv1 */
            card->card_type = CT_SD1;
            timeout = 1000;
            do {
                response = SD_SendCommand(card, ACMD41, 0);
                SD_CS_High(card);
                if (response == 0x00) break;
                osDelay(1);
            } while (--timeout);
        } else {
            /* MMCv3 */
            card->card_type = CT_MMC;
            timeout = 1000;
            do {
                response = SD_SendCommand(card, CMD1, 0);
                SD_CS_High(card);
                if (response == 0x00) break;
                osDelay(1);
            } while (--timeout);
        }

        if (timeout == 0) {
            card->card_type = 0;
            return SD_ERROR;
        }

        /* Set block length to 512 bytes for SDv1/MMC */
        response = SD_SendCommand(card, CMD16, SD_BLOCK_SIZE);
        SD_CS_High(card);
        if (response != 0x00) {
            card->card_type = 0;
            return SD_ERROR;
        }
    }

    if (card->card_type == 0) {
        return SD_ERROR;
    }

    /* Step 4: Increase SPI clock speed now that init is complete.
     * SPI1 APB2 clock = 108 MHz.
     * PRESCALER_4 = 108/4 = 27 MHz (within SD card max of 25 MHz for non-HS mode).
     * Use PRESCALER_8 = 13.5 MHz for conservative operation. */
    card->hspi->Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;  /* 13.5 MHz */
    HAL_SPI_Init(card->hspi);

    card->initialized = true;
    return SD_OK;
}

/**
 * @brief Read single 512-byte block from SD card
 */
SD_StatusTypeDef SD_ReadSingleBlock(SD_CardInfo *card, uint32_t sector, uint8_t *buffer) {
    uint8_t response;
    uint16_t retry;

    if (!card->initialized || !buffer) {
        return SD_INVALID_PARAMETER;
    }

    /* SDSC uses byte address; SDHC uses block address */
    if (!(card->card_type & CT_BLOCK)) {
        sector *= SD_BLOCK_SIZE;
    }

    /* CMD17: Read Single Block */
    response = SD_SendCommand(card, CMD17, sector);
    if (response != 0x00) {
        SD_CS_High(card);
        return SD_ERROR;
    }

    /* Wait for data token 0xFE */
    retry = 20000;
    do {
        response = SD_SPI_TransferByte(card, 0xFF);
        if (response == 0xFE) break;
    } while (--retry);

    if (response != 0xFE) {
        SD_CS_High(card);
        return SD_TIMEOUT;
    }

    /* Read 512 bytes of data */
    SD_SPI_ReadBytes(card, buffer, SD_BLOCK_SIZE);

    /* Discard 2-byte CRC */
    SD_SPI_TransferByte(card, 0xFF);
    SD_SPI_TransferByte(card, 0xFF);

    SD_CS_High(card);
    return SD_OK;
}

/**
 * @brief Write single 512-byte block to SD card
 */
SD_StatusTypeDef SD_WriteSingleBlock(SD_CardInfo *card, uint32_t sector, const uint8_t *buffer) {
    uint8_t response;

    if (!card->initialized || !buffer) {
        return SD_INVALID_PARAMETER;
    }

    if (!(card->card_type & CT_BLOCK)) {
        sector *= SD_BLOCK_SIZE;
    }

    /* CMD24: Write Single Block */
    response = SD_SendCommand(card, CMD24, sector);
    if (response != 0x00) {
        SD_CS_High(card);
        return SD_ERROR;
    }

    /* Data token */
    SD_SPI_TransferByte(card, 0xFE);

    /* Write 512 bytes */
    SD_SPI_WriteBytes(card, buffer, SD_BLOCK_SIZE);

    /* Dummy CRC */
    SD_SPI_TransferByte(card, 0xFF);
    SD_SPI_TransferByte(card, 0xFF);

    /* Data response: bits[4:0] = 0b00101 means accepted */
    response = SD_SPI_TransferByte(card, 0xFF);
    if ((response & 0x1F) != 0x05) {
        SD_CS_High(card);
        return SD_ERROR;
    }

    /* Wait for write to complete (card holds MISO low while busy) */
    if (!SD_WaitReady(card, SD_TIMEOUT)) {
        SD_CS_High(card);
        return SD_TIMEOUT;
    }

    SD_CS_High(card);
    return SD_OK;
}

/**
 * @brief Read multiple blocks from SD card
 */
SD_StatusTypeDef SD_ReadMultipleBlocks(SD_CardInfo *card, uint32_t sector, uint8_t *buffer, uint32_t count) {
    for (uint32_t i = 0; i < count; i++) {
        if (SD_ReadSingleBlock(card, sector + i, buffer + (i * SD_BLOCK_SIZE)) != SD_OK) {
            return SD_ERROR;
        }
    }
    return SD_OK;
}

/**
 * @brief Write multiple blocks to SD card
 */
SD_StatusTypeDef SD_WriteMultipleBlocks(SD_CardInfo *card, uint32_t sector, const uint8_t *buffer, uint32_t count) {
    for (uint32_t i = 0; i < count; i++) {
        if (SD_WriteSingleBlock(card, sector + i, buffer + (i * SD_BLOCK_SIZE)) != SD_OK) {
            return SD_ERROR;
        }
    }
    return SD_OK;
}

/**
 * @brief Get SD card sector count (simplified - reads CSD register)
 */
SD_StatusTypeDef SD_GetCardInfo(SD_CardInfo *card, uint32_t *sectors) {
    uint8_t csd[16];
    uint8_t response;

    if (!card->initialized || !sectors) {
        return SD_NOT_READY;
    }

    *sectors = 0;

    /* CMD9: Send CSD */
    response = SD_SendCommand(card, CMD9, 0);
    if (response != 0x00) {
        SD_CS_High(card);
        return SD_ERROR;
    }

    /* Wait for data token */
    uint16_t retry = 5000;
    do {
        response = SD_SPI_TransferByte(card, 0xFF);
        if (response == 0xFE) break;
    } while (--retry);

    if (response != 0xFE) {
        SD_CS_High(card);
        return SD_TIMEOUT;
    }

    /* Read 16 bytes CSD */
    SD_SPI_ReadBytes(card, csd, 16);
    SD_SPI_TransferByte(card, 0xFF);  /* CRC */
    SD_SPI_TransferByte(card, 0xFF);
    SD_CS_High(card);

    /* Parse sector count from CSD */
    if ((csd[0] >> 6) == 1) {
        /* CSD v2 (SDHC/SDXC) */
        uint32_t c_size = ((uint32_t)(csd[7] & 0x3F) << 16) |
                          ((uint32_t)csd[8] << 8) |
                          (uint32_t)csd[9];
        *sectors = (c_size + 1) * 1024;
    } else {
        /* CSD v1 (SDSC) */
        uint32_t c_size = ((uint32_t)(csd[6] & 0x03) << 10) |
                          ((uint32_t)csd[7] << 2) |
                          ((uint32_t)(csd[8] >> 6) & 0x03);
        uint8_t c_size_mult = ((csd[9] & 0x03) << 1) | ((csd[10] >> 7) & 0x01);
        uint8_t read_bl_len = csd[5] & 0x0F;
        *sectors = (c_size + 1) * (1 << (c_size_mult + 2)) * (1 << read_bl_len) / SD_BLOCK_SIZE;
    }

    return SD_OK;
}

/**
 * @brief Check if SD card is initialized
 */
bool SD_IsInitialized(SD_CardInfo *card) {
    return card ? card->initialized : false;
}
