/*
 * ST7735Driver.h
 */

#ifndef INC_ST7735DRIVER_H_
#define INC_ST7735DRIVER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f7xx.h"
#include <stdint.h>
#include <stdbool.h>

// DEFINES
typedef uint16_t ui16;
typedef void (*ST7735_DMA_Callback)(void);

#define ST7735_WIDTH    128
#define ST7735_HEIGHT   160

// COMMANDS
#define ST7735_NOP      0x00
#define ST7735_SWRESET  0x01
#define ST7735_SLPIN    0x10
#define ST7735_SLPOUT   0x11
#define ST7735_INVOFF   0x20
#define ST7735_INVON    0x21
#define ST7735_DISPOFF  0x28
#define ST7735_DISPON   0x29
#define ST7735_CASET    0x2A
#define ST7735_RASET    0x2B
#define ST7735_RAMWR    0x2C
#define ST7735_RAMRD    0x2E
#define ST7735_MADCTL   0x36
#define ST7735_COLMOD   0x3A

// COLORS (RGB565)
#define ST7735_BLACK    0x0000
#define ST7735_WHITE    0xFFFF
#define ST7735_RED      0xF800
#define ST7735_GREEN    0x07E0
#define ST7735_BLUE     0x001F
#define ST7735_CYAN     0x07FF
#define ST7735_MAGENTA  0xF81F
#define ST7735_YELLOW   0xFFE0
#define ST7735_ORANGE   0xFC00

typedef enum {
    ST7735_PORTRAIN = 0,
    ST7735_LANDSCAPE = 1,
    ST7735_PORTRAIT_INV = 2,
    ST7735_LANDSCAPE_INV = 3
} ST7735_Orientation_t;

typedef struct {
    SPI_HandleTypeDef *hspi;
    GPIO_TypeDef *cs_port;
    uint16_t cs_pin;

    GPIO_TypeDef *dc_port;
    uint16_t dc_pin;

    GPIO_TypeDef *rst_port;
    uint16_t rst_pin;

    GPIO_TypeDef *led_port;
    uint16_t led_pin;

    uint16_t width;
    uint16_t height;
    ST7735_Orientation_t orientation;
    ST7735_DMA_Callback dma_tx_complete_callback;

    TIM_HandleTypeDef *htim;
    uint32_t tim_channel;
} ST7735_Config_t;

// PUBLIC FUNCTIONS
void ST7735_Init(ST7735_Config_t *config);
void ST7735_Reset(void);

// Framebuffer functions
void ST7735_ClearFramebuffer(uint16_t color);
void ST7735_UpdateDisplay(void);  // Send framebuffer to display via DMA
void ST7735_SetPixelFB(ui16 x, ui16 y, ui16 color);  // Draw to framebuffer
uint16_t ST7735_GetPixelFB(ui16 x, ui16 y);  // Read from framebuffer

// Direct drawing functions (deprecated - use framebuffer versions)
void ST7735_FillScreen(uint16_t color);
void ST7735_DrawPixel(ui16 x, ui16 y, ui16 color);
void ST7735_SetLGBTBackground(void);
HAL_StatusTypeDef ST7735_DrawFRect(ui16 x, ui16 y, ui16 w, ui16 h, ui16 color);

// Framebuffer drawing functions
void ST7735_DrawLineFB(ui16 x0, ui16 y0, ui16 x1, ui16 y1, ui16 color);
void ST7735_DrawRectFB(ui16 x, ui16 y, ui16 w, ui16 h, ui16 color);
void ST7735_DrawFRectFB(ui16 x, ui16 y, ui16 w, ui16 h, ui16 color);
void ST7735_DrawCircleFB(ui16 x0, ui16 y0, ui16 r, ui16 color);
void ST7735_DrawFCircleFB(ui16 x0, ui16 y0, ui16 r, ui16 color);
void ST7735_DrawImageFB(ui16 x, ui16 y, ui16 w, ui16 h, const ui16 *data);

// Utility functions
void ST7735_SetRotation(ST7735_Orientation_t orientation);
void ST7735_InvertColors(bool invert);
void ST7735_DisplayOn(void);
void ST7735_DisplayOff(void);
void ST7735_SetLed(ST7735_Config_t *config, uint8_t value);

// Text rendering functions
void ST7735_DrawCharFB(ui16 x, ui16 y, char c, ui16 color, ui16 bg_color, uint8_t scale);
void ST7735_DrawStringFB(ui16 x, ui16 y, const char *str, ui16 color, ui16 bg_color, uint8_t scale);
void ST7735_DrawIntFB(ui16 x, ui16 y, int32_t num, ui16 color, ui16 bg_color, uint8_t scale);
void ST7735_DrawFloatFB(ui16 x, ui16 y, float num, uint8_t decimals, ui16 color, ui16 bg_color, uint8_t scale);
// Transparent text rendering (no background)
// Transparent text rendering (no background)
// wrap: 0 = no wrapping (continues off-screen), 1 = wrap to next line at display edge
void ST7735_DrawStringFB_Transparent(ui16 x, ui16 y, const char *str, ui16 color, uint8_t scale, uint8_t wrap);
void ST7735_DrawIntFB_Transparent(ui16 x, ui16 y, int32_t num, ui16 color, uint8_t scale, uint8_t wrap);
void ST7735_DrawFloatFB_Transparent(ui16 x, ui16 y, float num, uint8_t decimals, ui16 color, uint8_t scale, uint8_t wrap);

bool ST7735_IsBusy(void);
void ST7735_SetDMACallback(ST7735_DMA_Callback callback);
uint16_t ST7735_Color565(uint8_t r, uint8_t g, uint8_t b);

#ifdef __cplusplus
}
#endif

#endif /* INC_ST7735DRIVER_H_ */
