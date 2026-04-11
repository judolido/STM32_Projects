/**
 * @file sd_image_loader.c
 * @brief Implementation of SD card image loading functions
 */

#include "sd_image_loader.h"
#include "cmsis_os.h"
#include <string.h>
#include <stdio.h>

/* External semaphore for SD card access */
extern osSemaphoreId_t sdCardMutexHandle;

/**
 * @brief Load full screen image from SD card
 * @param filename Path to image file (raw RGB565 format)
 * @return true if successful, false otherwise
 */
bool SD_LoadFullScreenImage(const char *filename) {
    static FIL file;
    FRESULT fres;
    UINT bytes_read;
    
    /* Calculate expected file size */
    const uint32_t expected_size = ST7735_WIDTH * ST7735_HEIGHT * 2;  /* RGB565 = 2 bytes/pixel */
    
    /* Allocate buffer for one line at a time to save RAM */
    static uint16_t line_buffer[ST7735_WIDTH];
    
    /* Acquire SD card mutex */
    if (sdCardMutexHandle) {
        osSemaphoreAcquire(sdCardMutexHandle, osWaitForever);
    }
    
    /* Open file */
    fres = f_open(&file, filename, FA_READ);
    if (fres != FR_OK) {
    	//UART_Print("could not open the file\r\n");
        if (sdCardMutexHandle) osSemaphoreRelease(sdCardMutexHandle);
        return false;
    }
    //UART_Print("file opened\r\n");
    /* Check file size */
    if (f_size(&file) != expected_size) {
        f_close(&file);
        if (sdCardMutexHandle) osSemaphoreRelease(sdCardMutexHandle);
        return false;
    }
    
    /* Read and display line by line */
    for (uint16_t y = 0; y < ST7735_HEIGHT; y++) {
        fres = f_read(&file, line_buffer, ST7735_WIDTH * 2, &bytes_read);
        
        if (fres != FR_OK || bytes_read != ST7735_WIDTH * 2) {
            f_close(&file);
            if (sdCardMutexHandle) osSemaphoreRelease(sdCardMutexHandle);
            return false;
        }
        
        /* Draw line to framebuffer */
        for (uint16_t x = 0; x < ST7735_WIDTH; x++) {
            ST7735_SetPixelFB(x, y, line_buffer[x]);
        }
    }
    
    /* Close file */
    f_close(&file);
    
    /* Release mutex */
    if (sdCardMutexHandle) {
        osSemaphoreRelease(sdCardMutexHandle);
    }
    
    /* Update display */
    ST7735_UpdateDisplay();
    //UART_Print("display updated\r\n");
    return true;
}

/**
 * @brief Load partial image from SD card
 * @param filename Path to image file
 * @param x X position on display
 * @param y Y position on display
 * @param w Image width
 * @param h Image height
 * @return true if successful
 */
bool SD_LoadPartialImage(const char *filename, uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    static FIL file;
    FRESULT fres;
    UINT bytes_read;
    
    /* Validate dimensions */
    if (x + w > ST7735_WIDTH || y + h > ST7735_HEIGHT) {
        return false;
    }
    
    /* Allocate line buffer */
    static uint16_t line_buffer[128];  /* Max width */
    
    /* Acquire mutex */
    if (sdCardMutexHandle) {
        osSemaphoreAcquire(sdCardMutexHandle, osWaitForever);
    }
    
    /* Open file */
    fres = f_open(&file, filename, FA_READ);
    if (fres != FR_OK) {
        if (sdCardMutexHandle) osSemaphoreRelease(sdCardMutexHandle);
        return false;
    }
    
    /* Read and display line by line */
    for (uint16_t row = 0; row < h; row++) {
        fres = f_read(&file, line_buffer, w * 2, &bytes_read);
        
        if (fres != FR_OK || bytes_read != w * 2) {
            f_close(&file);
            if (sdCardMutexHandle) osSemaphoreRelease(sdCardMutexHandle);
            return false;
        }
        
        /* Draw line to framebuffer */
        for (uint16_t col = 0; col < w; col++) {
            ST7735_SetPixelFB(x + col, y + row, line_buffer[col]);
        }
    }
    
    /* Close file */
    f_close(&file);
    
    /* Release mutex */
    if (sdCardMutexHandle) {
        osSemaphoreRelease(sdCardMutexHandle);
    }
    
    /* Update display */
    ST7735_UpdateDisplay();
    
    return true;
}

/**
 * @brief Play animation from SD card
 * @param folder Folder containing frame_000.raw, frame_001.raw, etc.
 * @param frame_count Number of frames
 * @param fps Frames per second
 * @return true if successful
 */
bool SD_PlayAnimation(const char *folder, uint16_t frame_count, uint16_t fps) {
    char filename[64];
    uint32_t delay_ms = 1000 / fps;
    
    for (uint16_t frame = 0; frame < frame_count; frame++) {
        /* Construct filename */
        snprintf(filename, sizeof(filename), "%s/frame_%03d.raw", folder, frame);
        
        /* Load and display frame */
        if (!SD_LoadFullScreenImage(filename)) {
            return false;
        }
        
        /* Delay for frame rate */
        osDelay(delay_ms);
    }
    
    return true;
}

/**
 * @brief Get image information
 * @param filename Path to image file
 * @param info Pointer to ImageInfo structure
 * @return true if successful
 */
bool SD_GetImageInfo(const char *filename, ImageInfo_t *info) {
    FIL file;
    FRESULT fres;
    
    if (!info) return false;
    
    /* Acquire mutex */
    if (sdCardMutexHandle) {
        osSemaphoreAcquire(sdCardMutexHandle, osWaitForever);
    }
    
    /* Open file */
    fres = f_open(&file, filename, FA_READ);
    if (fres != FR_OK) {
        if (sdCardMutexHandle) osSemaphoreRelease(sdCardMutexHandle);
        return false;
    }
    
    /* Get file size */
    info->file_size = f_size(&file);
    
    /* Calculate dimensions (assuming square root for now) */
    /* For exact dimensions, you'd need a header in your image format */
    uint32_t pixel_count = info->file_size / 2;
    
    /* Assume common display sizes */
    if (pixel_count == 128 * 160) {
        info->width = 128;
        info->height = 160;
    } else if (pixel_count == 128 * 128) {
        info->width = 128;
        info->height = 128;
    } else {
        /* Unknown size */
        info->width = 0;
        info->height = 0;
    }
    
    /* Copy filename */
    strncpy(info->filename, filename, sizeof(info->filename) - 1);
    
    /* Close file */
    f_close(&file);
    
    /* Release mutex */
    if (sdCardMutexHandle) {
        osSemaphoreRelease(sdCardMutexHandle);
    }
    
    return true;
}

/**
 * @brief Create a test image on SD card (gradient pattern)
 * @param filename Output filename
 */
void SD_CreateTestImage(const char *filename) {
    static FIL file;
    FRESULT fres;
    UINT bytes_written;
    //uint16_t pixel;
    
    /* Acquire mutex */
    if (sdCardMutexHandle) {
    	//UART_Print("aquiring sd mutex\r\n");
        osSemaphoreAcquire(sdCardMutexHandle, osWaitForever);
        //UART_Print("sd mutex aquired\r\n");
    }
    
    /* Create file */
    //UART_Printf("SDPath = '%s'\r\n", SDPath);
    //UART_Printf("attempting f_open with path = '%s'\r\n", filename);
    fres = f_open(&file, filename, FA_WRITE | FA_CREATE_ALWAYS);
    if (fres != FR_OK) {
    	//UART_Print("file open failed\r\n");
    	//UART_Printf("error: %d \r\n", fres);
        if (sdCardMutexHandle) osSemaphoreRelease(sdCardMutexHandle);
        return;
    }
    //UART_Print("file opened/created succesfully\r\n");
    /* Generate gradient pattern */
    uint16_t row_buffer[ST7735_WIDTH];
    for (uint16_t y = 0; y < ST7735_HEIGHT; y++) {
        for (uint16_t x = 0; x < ST7735_WIDTH; x++) {
            uint8_t r = (x * 255) / ST7735_WIDTH;
            uint8_t g = (y * 255) / ST7735_HEIGHT;
            uint8_t b = ((x + y) * 255) / (ST7735_WIDTH + ST7735_HEIGHT);
            row_buffer[x] = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
        }
        f_write(&file, row_buffer, ST7735_WIDTH * 2, &bytes_written);
    }
    //UART_Print("gradient generated\r\n");
    /* Close file */
    f_close(&file);
    //UART_Print("file closed\r\n");
    /* Release mutex */
    if (sdCardMutexHandle) {
        osSemaphoreRelease(sdCardMutexHandle);
        //UART_Print("sd mutex released\r\n");
    }
}

/**
 * @brief Example: Load logo and display at center
 */
void Example_DisplayLogo(void) {
    /* Load logo from SD card */
    if (SD_LoadPartialImage("/logo.raw", 32, 48, 64, 64)) {
        /* Success */
    } else {
        /* Failed - display error text */
        ST7735_ClearFramebuffer(ST7735_BLACK);
        ST7735_DrawStringFB_Transparent(10, 70, "Logo not found", ST7735_RED, 1, 1);
        ST7735_UpdateDisplay();
    }
}

/**
 * @brief Example: Create and display test pattern
 */
void Example_CreateTestPattern(void) {
    /* Create test image */
	//UART_Print("testing test_gradient.raw\r\n");
	SD_CreateTestImage("0://root/gradient.raw"); // err 6


	//UART_Print("exited createimage\r\n");
    
    /* Display it */
    osDelay(100);
    //UART_Print("osdelayed100, loading full image\r\n");
    SD_LoadFullScreenImage("0://root/gradient.raw");
    //UART_Print("loaded full image\r\n");
}

/**
 * @brief Example: Play animation loop
 */
void Example_PlayAnimationLoop(void) {
    /* Assumes you have frame_000.raw through frame_029.raw in /anim/ folder */
    while (1) {
        SD_PlayAnimation("/anim", 30, 15);  /* 30 frames at 15 FPS */
        osDelay(500);  /* Pause before looping */
    }
}
