/**
 * @file sd_image_loader.h
 * @brief Helper functions to load and display images from SD card
 * 
 * IMAGE FORMAT:
 * - Raw RGB565 format (16-bit per pixel)
 * - No header, just pixel data
 * - Width x Height x 2 bytes
 * 
 * To create compatible images:
 * 1. Use image editing software (GIMP, Photoshop)
 * 2. Resize to 128x160 (or smaller)
 * 3. Export as raw RGB565 data
 * 
 * Or use Python:
 * ```python
 * from PIL import Image
 * import struct
 * 
 * img = Image.open('input.png').resize((128, 160))
 * img = img.convert('RGB')
 * 
 * with open('output.raw', 'wb') as f:
 *     for pixel in img.getdata():
 *         r, g, b = pixel
 *         rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
 *         f.write(struct.pack('<H', rgb565))
 * ```
 */

#ifndef SD_IMAGE_LOADER_H
#define SD_IMAGE_LOADER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f7xx_hal.h"
#include "ff.h"
#include "ST7735Driver.h"
#include <stdint.h>
#include <stdbool.h>

/* Image info structure */
typedef struct {
    uint16_t width;
    uint16_t height;
    uint32_t file_size;
    char filename[64];
} ImageInfo_t;

/* Function prototypes */
bool SD_LoadFullScreenImage(const char *filename);
bool SD_LoadPartialImage(const char *filename, uint16_t x, uint16_t y, uint16_t w, uint16_t h);
bool SD_PlayAnimation(const char *folder, uint16_t frame_count, uint16_t fps);
bool SD_GetImageInfo(const char *filename, ImageInfo_t *info);
void SD_CreateTestImage(const char *filename);
void Example_CreateTestPattern(void);

#ifdef __cplusplus
}
#endif

#endif /* SD_IMAGE_LOADER_H */
