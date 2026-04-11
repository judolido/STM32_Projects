/*
 * ST7735Driver.c
 */

#include "ST7735Driver.h"
#include <string.h>
#include <stdlib.h>

static ST7735_Config_t *display_config = NULL;
static volatile bool dma_transfer_in_progress = false;

// Framebuffer: RGB565 format (2 bytes per pixel)
#define FRAMEBUFFER_SIZE (ST7735_WIDTH * ST7735_HEIGHT)
static uint16_t framebuffer[FRAMEBUFFER_SIZE] __attribute__((aligned(4)));

// DMA buffer for transmission (byte array)
#define DMA_BUFFER_SIZE (FRAMEBUFFER_SIZE * 2)
static uint8_t dma_buffer[DMA_BUFFER_SIZE] __attribute__((aligned(4)));

// ==================== FONT DATA ====================
// Simple 5x7 font (ASCII 32-126)
static const uint8_t font_5x7[][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, // Space (32)
    {0x00, 0x00, 0x5F, 0x00, 0x00}, // !
    {0x00, 0x07, 0x00, 0x07, 0x00}, // "
    {0x14, 0x7F, 0x14, 0x7F, 0x14}, // #
    {0x24, 0x2A, 0x7F, 0x2A, 0x12}, // $
    {0x23, 0x13, 0x08, 0x64, 0x62}, // %
    {0x36, 0x49, 0x55, 0x22, 0x50}, // &
    {0x00, 0x05, 0x03, 0x00, 0x00}, // '
    {0x00, 0x1C, 0x22, 0x41, 0x00}, // (
    {0x00, 0x41, 0x22, 0x1C, 0x00}, // )
    {0x14, 0x08, 0x3E, 0x08, 0x14}, // *
    {0x08, 0x08, 0x3E, 0x08, 0x08}, // +
    {0x00, 0x50, 0x30, 0x00, 0x00}, // ,
    {0x08, 0x08, 0x08, 0x08, 0x08}, // -
    {0x00, 0x60, 0x60, 0x00, 0x00}, // .
    {0x20, 0x10, 0x08, 0x04, 0x02}, // /
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 0
    {0x00, 0x42, 0x7F, 0x40, 0x00}, // 1
    {0x42, 0x61, 0x51, 0x49, 0x46}, // 2
    {0x21, 0x41, 0x45, 0x4B, 0x31}, // 3
    {0x18, 0x14, 0x12, 0x7F, 0x10}, // 4
    {0x27, 0x45, 0x45, 0x45, 0x39}, // 5
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 6
    {0x01, 0x71, 0x09, 0x05, 0x03}, // 7
    {0x36, 0x49, 0x49, 0x49, 0x36}, // 8
    {0x06, 0x49, 0x49, 0x29, 0x1E}, // 9
    {0x00, 0x36, 0x36, 0x00, 0x00}, // :
    {0x00, 0x56, 0x36, 0x00, 0x00}, // ;
    {0x08, 0x14, 0x22, 0x41, 0x00}, //
    {0x14, 0x14, 0x14, 0x14, 0x14}, // =
    {0x00, 0x41, 0x22, 0x14, 0x08}, // >
    {0x02, 0x01, 0x51, 0x09, 0x06}, // ?
    {0x32, 0x49, 0x79, 0x41, 0x3E}, // @
    {0x7E, 0x11, 0x11, 0x11, 0x7E}, // A
    {0x7F, 0x49, 0x49, 0x49, 0x36}, // B
    {0x3E, 0x41, 0x41, 0x41, 0x22}, // C
    {0x7F, 0x41, 0x41, 0x22, 0x1C}, // D
    {0x7F, 0x49, 0x49, 0x49, 0x41}, // E
    {0x7F, 0x09, 0x09, 0x09, 0x01}, // F
    {0x3E, 0x41, 0x49, 0x49, 0x7A}, // G
    {0x7F, 0x08, 0x08, 0x08, 0x7F}, // H
    {0x00, 0x41, 0x7F, 0x41, 0x00}, // I
    {0x20, 0x40, 0x41, 0x3F, 0x01}, // J
    {0x7F, 0x08, 0x14, 0x22, 0x41}, // K
    {0x7F, 0x40, 0x40, 0x40, 0x40}, // L
    {0x7F, 0x02, 0x0C, 0x02, 0x7F}, // M
    {0x7F, 0x04, 0x08, 0x10, 0x7F}, // N
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, // O
    {0x7F, 0x09, 0x09, 0x09, 0x06}, // P
    {0x3E, 0x41, 0x51, 0x21, 0x5E}, // Q
    {0x7F, 0x09, 0x19, 0x29, 0x46}, // R
    {0x46, 0x49, 0x49, 0x49, 0x31}, // S
    {0x01, 0x01, 0x7F, 0x01, 0x01}, // T
    {0x3F, 0x40, 0x40, 0x40, 0x3F}, // U
    {0x1F, 0x20, 0x40, 0x20, 0x1F}, // V
    {0x3F, 0x40, 0x38, 0x40, 0x3F}, // W
    {0x63, 0x14, 0x08, 0x14, 0x63}, // X
    {0x07, 0x08, 0x70, 0x08, 0x07}, // Y
    {0x61, 0x51, 0x49, 0x45, 0x43}, // Z
    {0x00, 0x7F, 0x41, 0x41, 0x00}, // [
    {0x02, 0x04, 0x08, 0x10, 0x20}, // Backslash
    {0x00, 0x41, 0x41, 0x7F, 0x00}, // ]
    {0x04, 0x02, 0x01, 0x02, 0x04}, // ^
    {0x40, 0x40, 0x40, 0x40, 0x40}, // _
    {0x00, 0x01, 0x02, 0x04, 0x00}, // `
    {0x20, 0x54, 0x54, 0x54, 0x78}, // a
    {0x7F, 0x48, 0x44, 0x44, 0x38}, // b
    {0x38, 0x44, 0x44, 0x44, 0x20}, // c
    {0x38, 0x44, 0x44, 0x48, 0x7F}, // d
    {0x38, 0x54, 0x54, 0x54, 0x18}, // e
    {0x08, 0x7E, 0x09, 0x01, 0x02}, // f
    {0x0C, 0x52, 0x52, 0x52, 0x3E}, // g
    {0x7F, 0x08, 0x04, 0x04, 0x78}, // h
    {0x00, 0x44, 0x7D, 0x40, 0x00}, // i
    {0x20, 0x40, 0x44, 0x3D, 0x00}, // j
    {0x7F, 0x10, 0x28, 0x44, 0x00}, // k
    {0x00, 0x41, 0x7F, 0x40, 0x00}, // l
    {0x7C, 0x04, 0x18, 0x04, 0x78}, // m
    {0x7C, 0x08, 0x04, 0x04, 0x78}, // n
    {0x38, 0x44, 0x44, 0x44, 0x38}, // o
    {0x7C, 0x14, 0x14, 0x14, 0x08}, // p
    {0x08, 0x14, 0x14, 0x18, 0x7C}, // q
    {0x7C, 0x08, 0x04, 0x04, 0x08}, // r
    {0x48, 0x54, 0x54, 0x54, 0x20}, // s
    {0x04, 0x3F, 0x44, 0x40, 0x20}, // t
    {0x3C, 0x40, 0x40, 0x20, 0x7C}, // u
    {0x1C, 0x20, 0x40, 0x20, 0x1C}, // v
    {0x3C, 0x40, 0x30, 0x40, 0x3C}, // w
    {0x44, 0x28, 0x10, 0x28, 0x44}, // x
    {0x0C, 0x50, 0x50, 0x50, 0x3C}, // y
    {0x44, 0x64, 0x54, 0x4C, 0x44}, // z
    {0x00, 0x08, 0x36, 0x41, 0x00}, // {
    {0x00, 0x00, 0x7F, 0x00, 0x00}, // |
    {0x00, 0x41, 0x36, 0x08, 0x00}, // }
    {0x08, 0x04, 0x08, 0x10, 0x08}, // ~
};

#define FONT_WIDTH 5
#define FONT_HEIGHT 7
#define FONT_FIRST_CHAR 32  // Space
#define FONT_LAST_CHAR 126  // ~

// Private function prototypes
static void ST7735_WriteCommand(uint8_t cmd);
static void ST7735_WriteData(uint8_t data);
static void ST7735_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
static void ST7735_Select(void);
static void ST7735_Deselect(void);
static void ST7735_Delay(ui16 ms);

// ==================== HARDWARE CONTROL ====================
void ST7735_Reset(void) {
    if (display_config->rst_port != NULL) {
        HAL_GPIO_WritePin(display_config->rst_port, display_config->rst_pin, GPIO_PIN_RESET);
        ST7735_Delay(10);
        HAL_GPIO_WritePin(display_config->rst_port, display_config->rst_pin, GPIO_PIN_SET);
        ST7735_Delay(120);
    }
}

static void ST7735_Select(void) {
    HAL_GPIO_WritePin(display_config->cs_port, display_config->cs_pin, GPIO_PIN_RESET);
}

static void ST7735_Deselect(void) {
    HAL_GPIO_WritePin(display_config->cs_port, display_config->cs_pin, GPIO_PIN_SET);
}

static void ST7735_Delay(ui16 ms) {
    HAL_Delay(ms);
}

static void ST7735_WriteCommand(uint8_t cmd) {
    HAL_GPIO_WritePin(display_config->dc_port, display_config->dc_pin, GPIO_PIN_RESET);
    ST7735_Select();
    HAL_SPI_Transmit(display_config->hspi, &cmd, 1, HAL_MAX_DELAY);
    ST7735_Deselect();
}

static void ST7735_WriteData(uint8_t data) {
    HAL_GPIO_WritePin(display_config->dc_port, display_config->dc_pin, GPIO_PIN_SET);
    ST7735_Select();
    HAL_SPI_Transmit(display_config->hspi, &data, 1, HAL_MAX_DELAY);
    ST7735_Deselect();
}

static void ST7735_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    ST7735_WriteCommand(0x2A);  // Column address set
    ST7735_WriteData(x0 >> 8);
    ST7735_WriteData(x0 & 0xFF);
    ST7735_WriteData(x1 >> 8);
    ST7735_WriteData(x1 & 0xFF);

    ST7735_WriteCommand(0x2B);  // Row address set
    ST7735_WriteData(y0 >> 8);
    ST7735_WriteData(y0 & 0xFF);
    ST7735_WriteData(y1 >> 8);
    ST7735_WriteData(y1 & 0xFF);

    ST7735_WriteCommand(0x2C);  // Memory write
}

// ==================== INITIALIZATION ====================
void ST7735_Init(ST7735_Config_t *config) {
    display_config = config;
    dma_transfer_in_progress = false;

    // Start PWM for backlight
    HAL_TIM_PWM_Start(config->htim, config->tim_channel);
    ST7735_SetLed(config, 100);

    // Hardware reset
    ST7735_Reset();

    // Software reset
    ST7735_WriteCommand(0x01);
    HAL_Delay(150);

    // Sleep out
    ST7735_WriteCommand(0x11);
    HAL_Delay(120);

    // Frame rate control
    ST7735_WriteCommand(0xB1);
    ST7735_WriteData(0x01);
    ST7735_WriteData(0x2C);
    ST7735_WriteData(0x2D);

    // Pixel format: 16-bit color (RGB565)
    ST7735_WriteCommand(0x3A);
    ST7735_WriteData(0x05);

    // Memory data access control
    ST7735_WriteCommand(0x36);
    ST7735_WriteData(0x00);

    // Display on
    ST7735_WriteCommand(0x29);
    HAL_Delay(100);

    // Clear framebuffer and display
    ST7735_ClearFramebuffer(ST7735_BLACK);
    ST7735_UpdateDisplay();
}

// ==================== FRAMEBUFFER FUNCTIONS ====================
void ST7735_ClearFramebuffer(uint16_t color) {
    for (uint32_t i = 0; i < FRAMEBUFFER_SIZE; i++) {
        framebuffer[i] = color;
    }
}

void ST7735_SetLGBTBackground(void) {
    for (uint16_t y = 0; y < display_config->height; y++) {
        for (uint16_t x = 0; x < display_config->width; x++) {
            uint32_t index = y * display_config->width + x;

            uint8_t hue = (y * 255) / display_config->height;

            if (hue < 85) {
                framebuffer[index] = ST7735_Color565(255 - hue * 3, hue * 3, 0);
            } else if (hue < 170) {
                hue -= 85;
                framebuffer[index] = ST7735_Color565(0, 255 - hue * 3, hue * 3);
            } else {
                hue -= 170;
                framebuffer[index] = ST7735_Color565(hue * 3, 0, 255 - hue * 3);
            }
        }
    }
}

void ST7735_SetPixelFB(ui16 x, ui16 y, ui16 color) {
    if (x >= display_config->width || y >= display_config->height) {
        return;
    }

    uint32_t index = y * display_config->width + x;
    if (index < FRAMEBUFFER_SIZE) {
        framebuffer[index] = color;
    }
}

uint16_t ST7735_GetPixelFB(ui16 x, ui16 y) {
    if (x >= display_config->width || y >= display_config->height) {
        return 0;
    }

    uint32_t index = y * display_config->width + x;
    if (index < FRAMEBUFFER_SIZE) {
        return framebuffer[index];
    }
    return 0;
}

void ST7735_UpdateDisplay(void) {
    // Wait for any ongoing DMA transfer
    while (dma_transfer_in_progress) {
        // Busy wait
    }

    // Set window to entire screen
    ST7735_SetWindow(0, 0, display_config->width - 1, display_config->height - 1);

    // Convert framebuffer to byte array for DMA
    // RGB565 is big-endian on SPI
    for (uint32_t i = 0; i < FRAMEBUFFER_SIZE; i++) {
        dma_buffer[i * 2] = framebuffer[i] >> 8;        // High byte
        dma_buffer[i * 2 + 1] = framebuffer[i] & 0xFF;  // Low byte
    }

    // Send via DMA
    HAL_GPIO_WritePin(display_config->dc_port, display_config->dc_pin, GPIO_PIN_SET);
    ST7735_Select();

    dma_transfer_in_progress = true;
    HAL_SPI_Transmit_DMA(display_config->hspi, dma_buffer, DMA_BUFFER_SIZE);
}

// ==================== DMA CALLBACK ====================
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi) {
    if (hspi == display_config->hspi) {
        ST7735_Deselect();
        dma_transfer_in_progress = false;

        if (display_config->dma_tx_complete_callback != NULL) {
            display_config->dma_tx_complete_callback();
        }
    }
}

// ==================== FRAMEBUFFER DRAWING FUNCTIONS ====================
void ST7735_DrawLineFB(ui16 x0, ui16 y0, ui16 x1, ui16 y1, ui16 color) {
    // Bresenham's line algorithm
    int16_t dx = abs(x1 - x0);
    int16_t dy = abs(y1 - y0);
    int16_t sx = (x0 < x1) ? 1 : -1;
    int16_t sy = (y0 < y1) ? 1 : -1;
    int16_t err = dx - dy;
    int16_t e2;

    while (1) {
        ST7735_SetPixelFB(x0, y0, color);

        if (x0 == x1 && y0 == y1) break;

        e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void ST7735_DrawRectFB(ui16 x, ui16 y, ui16 w, ui16 h, ui16 color) {
    ST7735_DrawLineFB(x, y, x + w - 1, y, color);                   // Top
    ST7735_DrawLineFB(x, y + h - 1, x + w - 1, y + h - 1, color);   // Bottom
    ST7735_DrawLineFB(x, y, x, y + h - 1, color);                   // Left
    ST7735_DrawLineFB(x + w - 1, y, x + w - 1, y + h - 1, color);   // Right
}

void ST7735_DrawFRectFB(ui16 x, ui16 y, ui16 w, ui16 h, ui16 color) {
    for (ui16 j = 0; j < h; j++) {
        for (ui16 i = 0; i < w; i++) {
            ST7735_SetPixelFB(x + i, y + j, color);
        }
    }
}

void ST7735_DrawCircleFB(ui16 x0, ui16 y0, ui16 r, ui16 color) {
    int16_t x = r;
    int16_t y = 0;
    int16_t err = 0;

    while (x >= y) {
        ST7735_SetPixelFB(x0 + x, y0 + y, color);
        ST7735_SetPixelFB(x0 + y, y0 + x, color);
        ST7735_SetPixelFB(x0 - y, y0 + x, color);
        ST7735_SetPixelFB(x0 - x, y0 + y, color);
        ST7735_SetPixelFB(x0 - x, y0 - y, color);
        ST7735_SetPixelFB(x0 - y, y0 - x, color);
        ST7735_SetPixelFB(x0 + y, y0 - x, color);
        ST7735_SetPixelFB(x0 + x, y0 - y, color);

        if (err <= 0) {
            y += 1;
            err += 2 * y + 1;
        }
        if (err > 0) {
            x -= 1;
            err -= 2 * x + 1;
        }
    }
}

void ST7735_DrawFCircleFB(ui16 x0, ui16 y0, ui16 r, ui16 color) {
    int16_t x = r;
    int16_t y = 0;
    int16_t err = 0;

    while (x >= y) {
        // Draw horizontal lines
        for (int16_t i = x0 - x; i <= x0 + x; i++) {
            ST7735_SetPixelFB(i, y0 + y, color);
            ST7735_SetPixelFB(i, y0 - y, color);
        }
        for (int16_t i = x0 - y; i <= x0 + y; i++) {
            ST7735_SetPixelFB(i, y0 + x, color);
            ST7735_SetPixelFB(i, y0 - x, color);
        }

        if (err <= 0) {
            y += 1;
            err += 2 * y + 1;
        }
        if (err > 0) {
            x -= 1;
            err -= 2 * x + 1;
        }
    }
}

void ST7735_DrawImageFB(ui16 x, ui16 y, ui16 w, ui16 h, const ui16 *data) {
    for (ui16 j = 0; j < h; j++) {
        for (ui16 i = 0; i < w; i++) {
            ST7735_SetPixelFB(x + i, y + j, data[j * w + i]);
        }
    }
}

// ==================== LEGACY/DIRECT FUNCTIONS (for compatibility) ====================
void ST7735_FillScreen(uint16_t color) {
    ST7735_ClearFramebuffer(color);
    ST7735_UpdateDisplay();
}

void ST7735_DrawPixel(uint16_t x, uint16_t y, uint16_t color) {
    ST7735_SetPixelFB(x, y, color);
}

HAL_StatusTypeDef ST7735_DrawFRect(ui16 x, ui16 y, ui16 w, ui16 h, ui16 color) {
    ST7735_DrawFRectFB(x, y, w, h, color);
    return HAL_OK;
}

// ==================== UTILITY FUNCTIONS ====================
bool ST7735_IsBusy(void) {
    return dma_transfer_in_progress;
}

void ST7735_SetDMACallback(ST7735_DMA_Callback callback) {
    if (display_config != NULL) {
        display_config->dma_tx_complete_callback = callback;
    }
}

void ST7735_SetLed(ST7735_Config_t *config, uint8_t value) {
    uint32_t arr = __HAL_TIM_GET_AUTORELOAD(config->htim);
    uint32_t pulse = (arr * value) / 100;
    __HAL_TIM_SET_COMPARE(config->htim, config->tim_channel, pulse);
}

void ST7735_SetRotation(ST7735_Orientation_t orientation) {
    display_config->orientation = orientation;
    uint8_t madctl = 0;

    switch (orientation) {
        case ST7735_PORTRAIN:
            madctl = 0x00;
            display_config->width = ST7735_WIDTH;
            display_config->height = ST7735_HEIGHT;
            break;
        case ST7735_LANDSCAPE:
            madctl = 0x60;
            display_config->width = ST7735_HEIGHT;
            display_config->height = ST7735_WIDTH;
            break;
        case ST7735_PORTRAIT_INV:
            madctl = 0xC0;
            display_config->width = ST7735_WIDTH;
            display_config->height = ST7735_HEIGHT;
            break;
        case ST7735_LANDSCAPE_INV:
            madctl = 0xA0;
            display_config->width = ST7735_HEIGHT;
            display_config->height = ST7735_WIDTH;
            break;
    }

    ST7735_WriteCommand(ST7735_MADCTL);
    ST7735_WriteData(madctl);
}

void ST7735_InvertColors(bool invert) {
    ST7735_WriteCommand(invert ? ST7735_INVON : ST7735_INVOFF);
}

void ST7735_DisplayOn(void) {
    ST7735_WriteCommand(ST7735_DISPON);
}

void ST7735_DisplayOff(void) {
    ST7735_WriteCommand(ST7735_DISPOFF);
}

uint16_t ST7735_Color565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

// ==================== TEXT RENDERING ====================
void ST7735_DrawCharFB(ui16 x, ui16 y, char c, ui16 color, ui16 bg_color, uint8_t scale) {
    // Check if character is in font range
    if (c < FONT_FIRST_CHAR || c > FONT_LAST_CHAR) {
        c = '?';  // Replace with question mark
    }

    uint8_t char_index = c - FONT_FIRST_CHAR;

    // Draw character
    for (uint8_t col = 0; col < FONT_WIDTH; col++) {
        uint8_t line = font_5x7[char_index][col];

        for (uint8_t row = 0; row < 8; row++) {
            uint16_t pixel_color = (line & (1 << row)) ? color : bg_color;

            // Draw scaled pixel
            for (uint8_t sy = 0; sy < scale; sy++) {
                for (uint8_t sx = 0; sx < scale; sx++) {
                    ST7735_SetPixelFB(x + col * scale + sx,
                                     y + row * scale + sy,
                                     pixel_color);
                }
            }
        }
    }
}

void ST7735_DrawStringFB(ui16 x, ui16 y, const char *str, ui16 color, ui16 bg_color, uint8_t scale) {
    ui16 cursor_x = x;
    ui16 cursor_y = y;

    while (*str) {
        if (*str == '\n') {
            // Newline
            cursor_x = x;
            cursor_y += (FONT_HEIGHT + 1) * scale;
        } else if (*str == '\r') {
            // Carriage return
            cursor_x = x;
        } else {
            // Draw character
            ST7735_DrawCharFB(cursor_x, cursor_y, *str, color, bg_color, scale);
            cursor_x += (FONT_WIDTH + 1) * scale;  // +1 for spacing

            // Wrap to next line if needed
            if (cursor_x + (FONT_WIDTH * scale) > display_config->width) {
                cursor_x = x;
                cursor_y += (FONT_HEIGHT + 1) * scale;
            }
        }
        str++;
    }
}

void ST7735_DrawIntFB(ui16 x, ui16 y, int32_t num, ui16 color, ui16 bg_color, uint8_t scale) {
    char buffer[12];  // Enough for 32-bit int + sign + null

    // Convert integer to string
    sprintf(buffer, "%ld", (long)num);

    ST7735_DrawStringFB(x, y, buffer, color, bg_color, scale);
}

void ST7735_DrawFloatFB(ui16 x, ui16 y, float num, uint8_t decimals, ui16 color, ui16 bg_color, uint8_t scale) {
    char buffer[32];

    // Convert float to string with specified decimal places
    if (decimals == 0) {
        sprintf(buffer, "%.0f", num);
    } else if (decimals == 1) {
        sprintf(buffer, "%.1f", num);
    } else if (decimals == 2) {
        sprintf(buffer, "%.2f", num);
    } else if (decimals == 3) {
        sprintf(buffer, "%.3f", num);
    } else {
        sprintf(buffer, "%.4f", num);
    }

    ST7735_DrawStringFB(x, y, buffer, color, bg_color, scale);
}
void ST7735_DrawStringFB_Transparent(ui16 x, ui16 y, const char *str, ui16 color, uint8_t scale, uint8_t wrap) {
    ui16 cursor_x = x;
    ui16 cursor_y = y;

    while (*str) {
        if (*str == '\n') {
            // Newline
            cursor_x = x;
            cursor_y += (FONT_HEIGHT + 1) * scale;
        } else if (*str == '\r') {
            // Carriage return
            cursor_x = x;
        } else {
            // Only process valid characters
            if (*str >= FONT_FIRST_CHAR && *str <= FONT_LAST_CHAR) {
                uint8_t char_index = *str - FONT_FIRST_CHAR;

                // Draw character
                for (uint8_t col = 0; col < FONT_WIDTH; col++) {
                    uint8_t line = font_5x7[char_index][col];

                    for (uint8_t row = 0; row < 8; row++) {
                        if (line & (1 << row)) {  // Only draw foreground pixels
                            for (uint8_t sy = 0; sy < scale; sy++) {
                                for (uint8_t sx = 0; sx < scale; sx++) {
                                    ST7735_SetPixelFB(cursor_x + col * scale + sx,
                                                     cursor_y + row * scale + sy,
                                                     color);
                                }
                            }
                        }
                    }
                }
            }

            // Move cursor to next character position
            cursor_x += (FONT_WIDTH + 1) * scale;  // +1 for spacing

            // Wrap to next line if needed (only if wrap == 1)
            if (wrap == 1) {
                if (cursor_x + (FONT_WIDTH * scale) > display_config->width) {
                    cursor_x = x;
                    cursor_y += (FONT_HEIGHT + 1) * scale;
                }
            }
            // If wrap == 0, continue drawing off-screen (no wrapping)
        }
        str++;
    }
}




void ST7735_DrawIntFB_Transparent(ui16 x, ui16 y, int32_t num, ui16 color, uint8_t scale, uint8_t wrap) {
    char buffer[12];
    int index = 0;

    if (num < 0) {
        buffer[index++] = '-';
        num = -num;
    }

    if (num == 0) {
        buffer[index++] = '0';
    } else {
        char temp[12];
        int temp_idx = 0;

        while (num > 0) {
            temp[temp_idx++] = '0' + (num % 10);
            num /= 10;
        }

        for (int i = temp_idx - 1; i >= 0; i--) {
            buffer[index++] = temp[i];
        }
    }

    buffer[index] = '\0';

    ST7735_DrawStringFB_Transparent(x, y, buffer, color, scale, wrap);
}

void ST7735_DrawFloatFB_Transparent(ui16 x, ui16 y, float num, uint8_t decimals, ui16 color, uint8_t scale, uint8_t wrap) {
    char buffer[32];
    int index = 0;

    if (decimals > 6) decimals = 6;

    if (num < 0) {
        buffer[index++] = '-';
        num = -num;
    }

    float rounding = 0.5;
    for (uint8_t i = 0; i < decimals; i++) {
        rounding /= 10.0;
    }
    num += rounding;

    uint32_t int_part = (uint32_t)num;
    float frac_part = num - int_part;

    if (int_part == 0) {
        buffer[index++] = '0';
    } else {
        char temp[12];
        int temp_idx = 0;
        uint32_t n = int_part;

        while (n > 0) {
            temp[temp_idx++] = '0' + (n % 10);
            n /= 10;
        }

        for (int i = temp_idx - 1; i >= 0; i--) {
            buffer[index++] = temp[i];
        }
    }

    if (decimals > 0) {
        buffer[index++] = '.';

        for (uint8_t i = 0; i < decimals; i++) {
            frac_part *= 10.0;
            uint8_t digit = (uint8_t)frac_part;
            buffer[index++] = '0' + digit;
            frac_part -= digit;
        }
    }

    buffer[index] = '\0';

    ST7735_DrawStringFB_Transparent(x, y, buffer, color, scale, wrap);
}




