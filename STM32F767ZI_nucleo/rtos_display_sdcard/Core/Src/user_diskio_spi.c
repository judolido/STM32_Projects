/**
 * @file user_diskio_spi.c
 * @brief FatFs disk I/O driver for SD card via SPI
 *
 * Place this file in your project Src/ folder.
 * Disable the default user_diskio.c if CubeMX generated it.
 */

#include "ff_gen_drv.h"
#include "sd_card_spi.h"
#include <string.h>

/* External SD card object - defined in freertos_app.c */
extern SD_CardInfo sd_card;

/* Disk status */
static volatile DSTATUS Stat = STA_NOINIT;

/**
 * @brief Initializes the Drive
 * @param  pdrv: Physical drive number (0..)
 * @retval DSTATUS: Operation status
 */
DSTATUS USER_initialize(BYTE pdrv) {
    if (pdrv != 0) {
        return STA_NOINIT;
    }

    /* If already initialized (e.g. called manually before mount), skip re-init */
    if (sd_card.initialized) {
        Stat &= ~STA_NOINIT;
        return Stat;
    }

    if (SD_Init(&sd_card) == SD_OK) {
        Stat &= ~STA_NOINIT;
    } else {
        Stat = STA_NOINIT;
    }

    return Stat;
}

/**
 * @brief Gets Disk Status
 * @param  pdrv: Physical drive number (0..)
 * @retval DSTATUS: Operation status
 */
DSTATUS USER_status(BYTE pdrv) {
    if (pdrv != 0) {
        return STA_NOINIT;
    }

    /* Sync status with actual card state */
    if (sd_card.initialized) {
        Stat &= ~STA_NOINIT;
    } else {
        Stat = STA_NOINIT;
    }

    return Stat;
}

/**
 * @brief Reads Sector(s)
 * @param  pdrv:   Physical drive number (0..)
 * @param  buff:   Data buffer to store read data
 * @param  sector: Sector address (LBA)
 * @param  count:  Number of sectors to read
 * @retval DRESULT: Operation result
 */
DRESULT USER_read(BYTE pdrv, BYTE *buff, DWORD sector, UINT count) {
    if (pdrv != 0 || !count) {
        return RES_PARERR;
    }

    if (Stat & STA_NOINIT) {
        return RES_NOTRDY;
    }

    if (SD_ReadMultipleBlocks(&sd_card, sector, buff, count) == SD_OK) {
        return RES_OK;
    }

    return RES_ERROR;
}

/**
 * @brief Writes Sector(s)
 * @param  pdrv:   Physical drive number (0..)
 * @param  buff:   Data to be written
 * @param  sector: Sector address (LBA)
 * @param  count:  Number of sectors to write
 * @retval DRESULT: Operation result
 */
#if _USE_WRITE == 1
DRESULT USER_write(BYTE pdrv, const BYTE *buff, DWORD sector, UINT count) {
    if (pdrv != 0 || !count) {
        return RES_PARERR;
    }

    if (Stat & STA_NOINIT) {
        return RES_NOTRDY;
    }

    if (SD_WriteMultipleBlocks(&sd_card, sector, buff, count) == SD_OK) {
        return RES_OK;
    }

    return RES_ERROR;
}
#endif

/**
 * @brief I/O control operation
 * @param  pdrv: Physical drive number (0..)
 * @param  cmd:  Control code
 * @param  buff: Buffer to send/receive control data
 * @retval DRESULT: Operation result
 */
#if _USE_IOCTL == 1
DRESULT USER_ioctl(BYTE pdrv, BYTE cmd, void *buff) {
    DRESULT res = RES_ERROR;

    if (pdrv != 0) {
        return RES_PARERR;
    }

    if (Stat & STA_NOINIT) {
        return RES_NOTRDY;
    }

    switch (cmd) {
        case CTRL_SYNC:
            /* No write cache in this driver */
            res = RES_OK;
            break;

        case GET_SECTOR_COUNT: {
            /* FIX: Was returning 0, which causes f_mount to fail on some cards.
             * Now reads actual sector count from CSD register. */
            uint32_t sectors = 0;
            if (SD_GetCardInfo(&sd_card, &sectors) == SD_OK && sectors > 0) {
                *(DWORD*)buff = sectors;
                res = RES_OK;
            } else {
                /* Fallback: assume 2GB card (common minimum) */
                *(DWORD*)buff = 2UL * 1024UL * 1024UL * 1024UL / 512UL;
                res = RES_OK;
            }
            break;
        }

        case GET_SECTOR_SIZE:
            *(WORD*)buff = 512;
            res = RES_OK;
            break;

        case GET_BLOCK_SIZE:
            *(DWORD*)buff = 1;  /* Erase block size in sectors */
            res = RES_OK;
            break;

        default:
            res = RES_PARERR;
            break;
    }

    return res;
}
#endif

/* FatFs driver structure */
const Diskio_drvTypeDef USER_Driver = {
    USER_initialize,
    USER_status,
    USER_read,
#if _USE_WRITE == 1
    USER_write,
#endif
#if _USE_IOCTL == 1
    USER_ioctl,
#endif
};
