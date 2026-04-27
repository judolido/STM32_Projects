/**
 * @file freertos_app.c
 * @brief FreeRTOS application - SD card, display, and UART terminal
 *
 * DISPLAY FIX SUMMARY:
 *
 * Root cause of white screen: ST7735_Init() internally calls ST7735_UpdateDisplay()
 * which launches a non-blocking DMA transfer. The old code then called
 * ST7735_UpdateDisplay() AGAIN in FreeRTOS_AppInit() before the scheduler started,
 * causing a second DMA to fire while the first was still in flight (or right after),
 * corrupting the SPI bus. The display task then immediately called UpdateDisplay()
 * a third time before the DMA IRQ had a chance to fire (scheduler was not yet running
 * when the DMA from AppInit completed), leaving dma_transfer_in_progress stuck = true,
 * and the task spinning forever in the busy-wait inside UpdateDisplay().
 *
 * Fix:
 *   1. FreeRTOS_AppInit() calls ONLY ST7735_Init() - nothing else display-related.
 *      ST7735_Init() handles reset, init commands, and the first black fill via DMA.
 *   2. StartDisplayTask() waits for that initial DMA to finish using osDelay(1) in
 *      a loop - this yields the CPU so the DMA IRQ can actually fire and clear the
 *      busy flag. Then it draws the boot splash and enters the normal render loop.
 *   3. No display calls of any kind happen before osKernelStart().
 */

#include "main.h"
#include "cmsis_os.h"
#include "fatfs.h"
#include "ST7735Driver.h"
#include "sd_card_spi.h"
#include "uart_terminal.h"
#include "ff.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "sd_image_loader.h"
#include "SPI.h"

/* External handles from main.c */
extern SPI_HandleTypeDef hspi1;      /* SD Card SPI */
extern SPI_HandleTypeDef hspi3;      /* Display SPI */
extern UART_HandleTypeDef huart3;    /* UART terminal */
extern TIM_HandleTypeDef htim3;      /* Display PWM */

/* FreeRTOS handles */
osThreadId_t displayTaskHandle;
osThreadId_t sdCardTaskHandle;
osThreadId_t uartTaskHandle;

/* Queues and semaphores */
osMessageQueueId_t displayCommandQueueHandle;
osSemaphoreId_t sdCardMutexHandle;

/* SD Card object */
SD_CardInfo sd_card;

/* UART Terminal object */
UART_Terminal_t uart_terminal;

/*
 * General-Use SPI object
 */
Gen_SPI_info Gen_SPI;

/* FatFs objects - declared in CubeMX's fatfs.c, populated by MX_FATFS_Init().
 * USERPath is set to "0:" by FATFS_LinkDriver() at startup. */
extern char USERPath[4];
extern FATFS USERFatFS;
extern FIL USERFile;

/* Aliases so existing code continues to work without renaming every reference */
#define SDPath  USERPath
#define fs      USERFatFS
#define file    USERFile
static volatile uint8_t fps = 100;

/* Display configuration */
ST7735_Config_t display_config;

/* Display command structure */
typedef enum {
    DISP_CMD_CLEAR,
    DISP_CMD_TEXT,
    DISP_CMD_FILL_SCREEN,
    DISP_CMD_IMAGE,
    DISP_CMD_MAIN,
	DISP_CMD_FPS,
	DISP_CMD_PENDULUM,
	DISP_CMD_PATTERN
} DisplayCommand_t;

typedef struct {
    DisplayCommand_t cmd;
    uint16_t color;
    char text[64];
    uint16_t x, y;
    uint8_t fps;
} DisplayMessage_t;

typedef enum {
    DISPLAY_MODE_MAIN = 0,
    DISPLAY_MODE_TEXT,
    DISPLAY_MODE_FILL,
	DISPLAY_MODE_PENDULUM,
	DISPLAY_MODE_TESTPATTERN
} DisplayMode_t;

/* Display state */
static DisplayMode_t g_display_mode = DISPLAY_MODE_MAIN;
static char g_display_text[64] = "";
static uint16_t g_display_text_color = ST7735_WHITE;
static uint16_t g_display_fill_color = ST7735_BLACK;

/* Filesystem navigation state */
static char current_dir[64] = "0:/";

/* Cached SD card stats for main screen */
static DWORD sd_total_kb = 0;
static DWORD sd_free_kb = 0;
static uint32_t sd_stats_last_ms = 0;
static uint8_t cpu_usage_percent = 0;

/*	for the pendulum					 */
static float alpha = 0;
static uint8_t forward = 1;
/* Function prototypes */
void StartDisplayTask(void *argument);
void StartSPITask(void *argument);
void StartSDCardTask(void *argument);
void StartUARTTask(void *argument);

static void BuildPath(const char *input, char *out, size_t out_size);
static void UpdateSdStats(void);
uint32_t FreeRTOS_GetIdleCounter(void);

/* UART Command handlers */
void CMD_LS(int argc, char *argv[]);
void CMD_CD(int argc, char *argv[]);
void CMD_PWD(int argc, char *argv[]);
void CMD_Cat(int argc, char *argv[]);
void CMD_Write(int argc, char *argv[]);
void CMD_Mkdir(int argc, char *argv[]);
void CMD_RM(int argc, char *argv[]);
void CMD_Clear(int argc, char *argv[]);
void CMD_Text(int argc, char *argv[]);
void CMD_Fill(int argc, char *argv[]);
void CMD_Home(int argc, char *argv[]);
void CMD_Info(int argc, char *argv[]);
void CMD_Uptime(int argc, char *argv[]);
void CMD_Format(int argc, char *argv[]);
void CMD_Debug(int argc, char *argv[]);
void CMD_CardState(int argc, char *argv[]);
void CMD_TestMount(int argc, char *argv[]);
void CMD_TestSector0(int argc, char *argv[]);
void CMD_TestMultiRead(int argc, char *argv[]);
void CMD_SetFPS(int argc, char *argv[]);
void CMD_Pendulum(int argc, char *argv[]);
void CMD_Reinit(int argc, char *argv[]);
void CMD_Pattern(int argc, char *argv[]);

/* Command table */
const UART_Command_t commands[] = {
    {"ls",        "List files in current dir",           CMD_LS},
    {"cd",        "Change directory",                    CMD_CD},
    {"pwd",       "Print current directory",             CMD_PWD},
    {"cat",       "Display file contents",               CMD_Cat},
    {"write",     "Write to file (write filename text)", CMD_Write},
    {"mkdir",     "Create directory",                    CMD_Mkdir},
    {"rm",        "Remove file",                         CMD_RM},
    {"clear",     "Clear display",                       CMD_Clear},
    {"text",      "Display text (text \"message\")",     CMD_Text},
    {"fill",      "Fill screen (fill color)",            CMD_Fill},
    {"home",      "Show main status display",            CMD_Home},
    {"info",      "Show SD card info",                   CMD_Info},
    {"uptime",    "Show system uptime",                  CMD_Uptime},
    {"format",    "Format SD card (CAUTION!)",           CMD_Format},
    {"debug",     "Show SD card state",                  CMD_Debug},
    {"cardstate", "Show SD card state",                  CMD_CardState},
    {"testmount", "Test filesystem mount",               CMD_TestMount},
    {"sector0",   "Read and analyze sector 0",           CMD_TestSector0},
    {"multiread", "Test reading multiple sectors",       CMD_TestMultiRead},
    {"reinit",    "Re-initialize SD card",               CMD_Reinit},
	{"setfps",    "Set Display FPS",               CMD_SetFPS},
	{"pendulum",    "Drawing a pendulum",               CMD_Pendulum},
	{"gradient",    "Drawing a gradient",               CMD_Pattern},
};

/* ==================== INTERNAL HELPERS ==================== */

static void BuildPath(const char *input, char *out, size_t out_size) {
    if (!input || input[0] == '\0' || strcmp(input, ".") == 0) {
        strncpy(out, current_dir, out_size);
        out[out_size - 1] = '\0';
        return;
    }

    /* Absolute path (explicit drive or leading slash) */
    if (strchr(input, ':')) {
        strncpy(out, input, out_size);
        out[out_size - 1] = '\0';
        return;
    }

    if (input[0] == '/' && strncmp(current_dir, "0:", 2) == 0) {
        /* Relative to root of current drive */
        snprintf(out, out_size, "0:%s", input);
    } else {
        /* Child of current_dir */
        snprintf(out, out_size, "%s/%s", current_dir, input);
    }
    out[out_size - 1] = '\0';
}

static void UpdateSdStats(void) {
    if (!sd_card.initialized) {
        sd_total_kb = 0;
        sd_free_kb = 0;
        return;
    }

    uint32_t now = HAL_GetTick();
    if ((now - sd_stats_last_ms) < 5000U) {
        return; /* Refresh every 5 seconds at most */
    }
    sd_stats_last_ms = now;

    osSemaphoreAcquire(sdCardMutexHandle, osWaitForever);

    DWORD fre_clust, fre_sect, tot_sect;
    FATFS *fs_ptr = &fs;
    if (f_getfree(SDPath, &fre_clust, &fs_ptr) == FR_OK) {
        tot_sect = (fs.n_fatent - 2) * fs.csize;
        fre_sect = fre_clust * fs.csize;
        sd_total_kb = tot_sect / 2;
        sd_free_kb = fre_sect / 2;
    }

    osSemaphoreRelease(sdCardMutexHandle);
}

/* ==================== APP INIT ==================== */

/**
 * @brief Initialize FreeRTOS application.
 *
 * Called from main.c AFTER osKernelInitialize() and BEFORE osKernelStart().
 *
 * IMPORTANT: Do NOT call any ST7735 drawing or UpdateDisplay functions here.
 * ST7735_Init() already launches a DMA transfer internally to clear the screen.
 * That DMA will complete once the scheduler starts and the IRQ can fire.
 * All further display operations belong in StartDisplayTask().
 */
void FreeRTOS_AppInit(void) {
    /* --- SD card config (no init yet - happens in StartSDCardTask) --- */
    sd_card.hspi = &hspi1;
    sd_card.cs_port = GPIOC;
    sd_card.cs_pin = GPIO_PIN_11;
    sd_card.initialized = false;

    /* --- Display config --- */
    display_config.hspi = &hspi3;
    display_config.cs_port = GPIOD;
    display_config.cs_pin = GPIO_PIN_7;
    display_config.dc_port = GPIOD;
    display_config.dc_pin = GPIO_PIN_5;
    display_config.rst_port = GPIOD;
    display_config.rst_pin = GPIO_PIN_6;
    display_config.led_port = GPIOA;
    display_config.led_pin = GPIO_PIN_6;
    display_config.width = 128;
    display_config.height = 160;
    display_config.htim = &htim3;
    display_config.tim_channel = TIM_CHANNEL_1;
    display_config.dma_tx_complete_callback = NULL;

    /* --- GEN SPI2 config --- */
    Gen_SPI.cs_pin = GPIO_PIN_4;
    Gen_SPI.cs_port = GPIOA;
	Gen_SPI.hspi = &hspi2;
    /*
     * ST7735_Init() does:
     *   - Starts PWM backlight
     *   - Hardware + software reset (blocking HAL_Delay calls)
     *   - Sends all init commands (blocking SPI)
     *   - ClearFramebuffer(BLACK) + UpdateDisplay() via DMA  <-- non-blocking DMA
     *
     * The DMA transfer launched here will complete once the RTOS scheduler
     * starts and enables the DMA interrupt to fire. Do not touch the display
     * again until StartDisplayTask() confirms ST7735_IsBusy() == false.
     */
    ST7735_Init(&display_config);

    /* --- UART terminal --- */
    UART_Terminal_Init(&uart_terminal, &huart3);
    UART_Terminal_RegisterCommands(commands, sizeof(commands) / sizeof(commands[0]));

    /* --- RTOS objects (valid after osKernelInitialize()) --- */
    const osSemaphoreAttr_t sdCardMutex_attributes = {.name = "sdCardMutex"};
    sdCardMutexHandle = osSemaphoreNew(1, 1, &sdCardMutex_attributes);

    const osMessageQueueAttr_t displayQueue_attributes = {.name = "displayQueue"};
    displayCommandQueueHandle = osMessageQueueNew(8, sizeof(DisplayMessage_t), &displayQueue_attributes);

    /* --- Create tasks --- */
    const osThreadAttr_t displayTask_attributes = {
        .name = "displayTask",
        .stack_size = 1024 * 4,
        .priority = (osPriority_t) osPriorityNormal,
    };
    displayTaskHandle = osThreadNew(StartDisplayTask, NULL, &displayTask_attributes);

    const osThreadAttr_t SPITask_attributes = {
        .name = "displayTask",
        .stack_size = 1024 * 4,
        .priority = (osPriority_t) osPriorityNormal,
    };
    displayTaskHandle = osThreadNew(StartSPITask, NULL, &SPITask_attributes);

    const osThreadAttr_t sdCardTask_attributes = {
        .name = "sdCardTask",
        .stack_size = 1024 * 4,
        .priority = (osPriority_t) osPriorityNormal,
    };
    sdCardTaskHandle = osThreadNew(StartSDCardTask, NULL, &sdCardTask_attributes);

    const osThreadAttr_t uartTask_attributes = {
        .name = "uartTask",
        .stack_size = 512 * 4,
        .priority = (osPriority_t) osPriorityNormal,
    };
    uartTaskHandle = osThreadNew(StartUARTTask, NULL, &uartTask_attributes);
}

/* ==================== TASK IMPLEMENTATIONS ==================== */

/**
 * @brief Display task - handles all screen rendering.
 *
 * Starts by waiting for the DMA transfer launched inside ST7735_Init() to
 * complete. Because the scheduler is now running, the DMA IRQ can fire and
 * clear the busy flag. We yield with osDelay(1) instead of spinning so other
 * tasks and interrupts get CPU time.
 */
void StartSPITask(void *argument) {

}

void StartDisplayTask(void *argument) {
    uint32_t counter = 0;

    /*
     * Wait for the initial DMA (started inside ST7735_Init) to finish.
     * osDelay(1) yields the CPU so the DMA IRQ can actually fire.
     * Without this, the task would spin forever in the busy-wait inside
     * the next call to ST7735_UpdateDisplay().
     */
    while (ST7735_IsBusy()) {
        osDelay(1);
    }

    /* Boot splash - DMA is free, scheduler is running */
    ST7735_ClearFramebuffer(ST7735_BLACK);
    ST7735_DrawStringFB(10, 10, "Booting...", ST7735_WHITE, ST7735_BLACK, 1);
    ST7735_UpdateDisplay();
    osDelay(500);   /* Let DMA finish and show splash for half a second */

    UART_Print("Display task started\r\n");

    for (;;) {
        /* Apply any pending display command from the queue (non-blocking peek) */
        DisplayMessage_t msg;
        if (osMessageQueueGet(displayCommandQueueHandle, &msg, NULL, 0) == osOK) {
            switch (msg.cmd) {
                case DISP_CMD_CLEAR:
                    g_display_mode = DISPLAY_MODE_TEXT;
                    g_display_text[0] = '\0';
                    g_display_text_color = ST7735_WHITE;
                    break;
                case DISP_CMD_TEXT:
                    g_display_mode = DISPLAY_MODE_TEXT;
                    strncpy(g_display_text, msg.text, sizeof(g_display_text) - 1);
                    g_display_text[sizeof(g_display_text) - 1] = '\0';
                    g_display_text_color = msg.color;
                    break;
                case DISP_CMD_FILL_SCREEN:
                    g_display_mode = DISPLAY_MODE_FILL;
                    g_display_fill_color = msg.color;
                    break;
                case DISP_CMD_MAIN:
                    g_display_mode = DISPLAY_MODE_MAIN;
                    break;
                case DISP_CMD_PENDULUM:
                	g_display_mode = DISPLAY_MODE_PENDULUM;
                	break;
                case DISP_CMD_PATTERN:
                	g_display_mode = DISPLAY_MODE_TESTPATTERN;
                	break;
                default:
                    break;
            }
        }

        char buf[32];

        switch (g_display_mode) {
            case DISPLAY_MODE_MAIN: {
                ST7735_ClearFramebuffer(ST7735_BLACK);

                snprintf(buf, sizeof(buf), "Counter: %lu", counter++);
                ST7735_DrawStringFB_Transparent(5, 10, buf, ST7735_GREEN, 1, 1);

                uint32_t seconds = HAL_GetTick() / 1000U;
                snprintf(buf, sizeof(buf), "Up: %lu:%02lu",
                         (unsigned long)(seconds / 60U),
                         (unsigned long)(seconds % 60U));
                ST7735_DrawStringFB_Transparent(5, 25, buf, ST7735_YELLOW, 1, 1);

                if (sd_card.initialized) {
                    UpdateSdStats();
                    snprintf(buf, sizeof(buf), "SD: %lu/%lu KB",
                             (unsigned long)sd_free_kb,
                             (unsigned long)sd_total_kb);
                    ST7735_DrawStringFB_Transparent(5, 40, buf, ST7735_CYAN, 1, 1);
                } else {
                    ST7735_DrawStringFB_Transparent(5, 40, "SD: FAIL", ST7735_RED, 1, 1);
                }

                /* CPU usage estimate - updated once per second */
                static uint32_t last_idle = 0;
                static uint32_t last_ms = 0;
                uint32_t now_ms = HAL_GetTick();
                uint32_t idle_now = FreeRTOS_GetIdleCounter();
                uint32_t dt = now_ms - last_ms;
                if (dt >= 1000U && dt <= 5000U) {
                    uint32_t idle_delta = idle_now - last_idle;
                    static uint32_t idle_max = 0;
                    if (idle_delta > idle_max) {
                        idle_max = idle_delta;
                    }
                    if (idle_max > 0) {
                        uint32_t load = 100U - (idle_delta * 100U / idle_max);
                        if (load > 100U) load = 100U;
                        cpu_usage_percent = (uint8_t)load;
                    }
                    last_idle = idle_now;
                    last_ms = now_ms;
                } else if (last_ms == 0U) {
                    last_ms = now_ms;
                    last_idle = idle_now;
                }

                snprintf(buf, sizeof(buf), "CPU: %3u%%", cpu_usage_percent);
                ST7735_DrawStringFB_Transparent(5, 55, buf, ST7735_WHITE, 1, 1);
                ST7735_DrawStringFB_Transparent(5, 70, "Type 'help'", ST7735_WHITE, 1, 1);
                break;
            }

            case DISPLAY_MODE_TEXT:
                ST7735_ClearFramebuffer(ST7735_BLACK);
                if (g_display_text[0] != '\0') {
                    ST7735_DrawStringFB_Transparent(5, 10, g_display_text, g_display_text_color, 1, 1);
                }
                break;

            case DISPLAY_MODE_FILL:
                ST7735_ClearFramebuffer(g_display_fill_color);
                break;
            case DISPLAY_MODE_PENDULUM:
            	if (forward) {
            		ST7735_ClearFramebuffer(ST7735_BLACK);
            		            	int x0L = ST7735_WIDTH/2;
            		            	int y0L = 30;
            		            	int x1L = ST7735_WIDTH/2 -30 + (1- (pow(alpha,2)/2) + (pow(alpha, 4)/24));
            		            	int xc = x1L;
            		            	int y1L = 60 + (alpha - pow(alpha,3)/6 + pow(alpha, 5)/120);
            		            	int yc = y1L;

            		            	ST7735_DrawLineFB(x0L, y0L, x1L, y1L, ST7735_RED);
            		            	ST7735_DrawFCircleFB(xc, yc, 10, ST7735_RED);

            		            	alpha+= 2;
            		            	if (alpha == 100){
            		            		forward = 0;
            		            	}
            	} else {
            		ST7735_ClearFramebuffer(ST7735_BLACK);
            		            	int x0L = ST7735_WIDTH/2;
            		            	int y0L = 30;
            		            	int x1L = ST7735_WIDTH/2 -30 + (1- (pow(alpha,2)/2) + (pow(alpha, 4)/24));
            		            	int xc = x1L;
            		            	int y1L = 60 + (alpha - (pow(alpha,3)/6) + (pow(alpha, 5)/120));
            		            	int yc = y1L;

            		            	ST7735_DrawLineFB(x0L, y0L, x1L, y1L, ST7735_RED);
            		            	ST7735_DrawFCircleFB(xc, yc, 10, ST7735_RED);

            		            	alpha-=2;
            		            	if (alpha == 0){
            		            		forward = 1;
            		            	}

            	}

            	break;
            case DISPLAY_MODE_TESTPATTERN:
            	//UART_Print("created example \r\n");
            	Example_CreateTestPattern();
            	//UART_Print("exited example \r\n");
            	break;
            default:
                ST7735_ClearFramebuffer(ST7735_BLACK);
                break;
        }

        /*
         * Wait for any previous DMA to finish before starting a new one.
         * osDelay(1) keeps yielding until the DMA IRQ clears the busy flag.
         */
        while (ST7735_IsBusy()) {
            osDelay(1);
        }
        ST7735_UpdateDisplay();

        osDelay(fps);  /* ~10 FPS */
    }
}

/**
 * @brief SD card task - initializes SD card and mounts filesystem.
 */
void StartSDCardTask(void *argument) {
    FRESULT fres;

    UART_Print("SD Card task started\r\n");

    /* Wait for system to settle after boot */
    osDelay(500);

    UART_Print("\r\n========================================\r\n");
    UART_Print("  SD Card Initialization\r\n");
    UART_Print("  Pins: CS=PC11  SCK=PA5  MOSI=PA7  MISO=PG9\r\n");
    UART_Print("  SPI:  SPI1, Mode 0 (CPOL=0, CPHA=0)\r\n");
    UART_Print("========================================\r\n");

    osSemaphoreAcquire(sdCardMutexHandle, osWaitForever);

    UART_Print("Initializing SD card...\r\n");
    SD_StatusTypeDef sd_result = SD_Init(&sd_card);

    if (sd_result == SD_OK) {
        UART_Printf("SD card OK! Type: 0x%02X", sd_card.card_type);
        if (sd_card.card_type & CT_SD2)   UART_Print(" SDv2");
        if (sd_card.card_type & CT_SD1)   UART_Print(" SDv1");
        if (sd_card.card_type & CT_MMC)   UART_Print(" MMC");
        if (sd_card.card_type & CT_BLOCK) UART_Print(" SDHC");
        UART_Print("\r\n");

        UART_Print("Mounting FAT filesystem...\r\n");
        fres = f_mount(&fs, SDPath, 1);

        if (fres == FR_OK) {
            DWORD fre_clust, fre_sect, tot_sect;
            FATFS *fs_ptr = &fs;

            UART_Print("Filesystem mounted!\r\n");

            fres = f_getfree(SDPath, &fre_clust, &fs_ptr);
            if (fres == FR_OK) {
                tot_sect = (fs.n_fatent - 2) * fs.csize;
                fre_sect = fre_clust * fs.csize;
                UART_Printf("Total: %lu KB, Free: %lu KB\r\n",
                            tot_sect / 2, fre_sect / 2);
            }

            UART_Print("\r\n========================================\r\n");
            UART_Print("  SD Card Ready! Type 'help' for commands\r\n");
            UART_Print("========================================\r\n");
        } else {
            UART_Printf("Mount failed: %d  ", fres);
            switch (fres) {
                case FR_NO_FILESYSTEM: UART_Print("(No FAT filesystem - format card as FAT32)\r\n"); break;
                case FR_DISK_ERR:      UART_Print("(Disk I/O error - check wiring)\r\n"); break;
                case FR_NOT_READY:     UART_Print("(Not ready - diskio init failed)\r\n"); break;
                default:               UART_Print("\r\n"); break;
            }
            UART_Print("Type 'sector0' to inspect raw card data\r\n");
        }
    } else {
        UART_Print("SD card initialization FAILED!\r\n\r\n");
        UART_Print("Checklist:\r\n");
        UART_Print("  1. Wiring: CS=PC11  SCK=PA5  MOSI=PA7  MISO=PG9\r\n");
        UART_Print("  2. Power: 3.3V to SD module VCC\r\n");
        UART_Print("  3. Card inserted firmly\r\n");
        UART_Print("  4. Card is FAT32 formatted (2GB-32GB)\r\n");
        UART_Print("  5. Try different SD card\r\n");
        UART_Print("\r\nType 'reinit' to retry initialization\r\n");
    }

    osSemaphoreRelease(sdCardMutexHandle);

    /* Task finished - suspend, commands handled via UART */
    vTaskSuspend(NULL);
}

/**
 * @brief UART task - processes received characters in task context.
 */
void StartUARTTask(void *argument) {
    UART_Print("UART task started\r\n");
    UART_Terminal_TaskLoop();
}

/* ==================== UART COMMAND HANDLERS ==================== */

void CMD_LS(int argc, char *argv[]) {
    DIR dir;
    FILINFO fno;
    FRESULT fres;

    char path[64];
    if (argc > 1) {
        BuildPath(argv[1], path, sizeof(path));
    } else {
        strncpy(path, current_dir, sizeof(path));
        path[sizeof(path) - 1] = '\0';
    }

    osSemaphoreAcquire(sdCardMutexHandle, osWaitForever);

    fres = f_opendir(&dir, path);
    if (fres == FR_OK) {
        UART_Printf("\r\nDirectory: '%s'\r\n", path);
        UART_Print("----------------------------------------\r\n");

        while (1) {
            fres = f_readdir(&dir, &fno);
            if (fres != FR_OK || fno.fname[0] == 0) break;

            if (fno.fattrib & AM_DIR) {
                UART_Printf("[DIR]  %s\r\n", fno.fname);
            } else {
                UART_Printf("[FILE] %-20s %lu bytes\r\n", fno.fname, fno.fsize);
            }
        }
        f_closedir(&dir);
        UART_Print("----------------------------------------\r\n");
    } else {
        UART_Printf("Failed to open directory '%s': error %d\r\n", path, fres);
    }

    osSemaphoreRelease(sdCardMutexHandle);
}

void CMD_CD(int argc, char *argv[]) {
    if (argc < 2) {
        UART_Printf("Current dir: %s\r\n", current_dir);
        return;
    }

    char target[64];

    if (strcmp(argv[1], "..") == 0) {
        strncpy(target, current_dir, sizeof(target));
        target[sizeof(target) - 1] = '\0';
        size_t len = strlen(target);
        if (len > 3) {
            char *slash = strrchr(target, '/');
            if (slash && (slash - target) >= 3) {
                *slash = '\0';
            }
        }
    } else {
        BuildPath(argv[1], target, sizeof(target));
    }

    osSemaphoreAcquire(sdCardMutexHandle, osWaitForever);
    DIR dir;
    FRESULT fres = f_opendir(&dir, target);
    if (fres == FR_OK) {
        f_closedir(&dir);
        strncpy(current_dir, target, sizeof(current_dir));
        current_dir[sizeof(current_dir) - 1] = '\0';
        UART_Printf("Changed directory to '%s'\r\n", current_dir);
    } else {
        UART_Printf("cd: cannot access '%s': error %d\r\n", target, fres);
    }
    osSemaphoreRelease(sdCardMutexHandle);
}

void CMD_PWD(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    UART_Printf("Current dir: %s\r\n", current_dir);
}

void CMD_Cat(int argc, char *argv[]) {
    if (argc < 2) {
        UART_Print("Usage: cat <filename>\r\n");
        return;
    }

    FRESULT fres;
    UINT bytes_read;
    char buffer[128];
    char path[64];

    BuildPath(argv[1], path, sizeof(path));

    osSemaphoreAcquire(sdCardMutexHandle, osWaitForever);

    fres = f_open(&file, path, FA_READ);
    if (fres == FR_OK) {
        UART_Printf("\r\n--- %s ---\r\n", path);
        while (1) {
            fres = f_read(&file, buffer, sizeof(buffer) - 1, &bytes_read);
            if (fres != FR_OK || bytes_read == 0) break;
            buffer[bytes_read] = '\0';
            UART_Print(buffer);
        }
        f_close(&file);
        UART_Print("\r\n--- End ---\r\n");
    } else {
        UART_Printf("Cannot open '%s': error %d\r\n", path, fres);
    }

    osSemaphoreRelease(sdCardMutexHandle);
}

void CMD_Write(int argc, char *argv[]) {
    if (argc < 3) {
        UART_Print("Usage: write <filename> <text>\r\n");
        return;
    }

    FRESULT fres;
    UINT bytes_written;
    char text[128] = "";
    char path[64];

    for (int i = 2; i < argc; i++) {
        if (strlen(text) + strlen(argv[i]) + 2 < sizeof(text)) {
            strcat(text, argv[i]);
            if (i < argc - 1) strcat(text, " ");
        }
    }
    strcat(text, "\r\n");

    osSemaphoreAcquire(sdCardMutexHandle, osWaitForever);

    BuildPath(argv[1], path, sizeof(path));

    fres = f_open(&file, path, FA_WRITE | FA_OPEN_APPEND | FA_CREATE_ALWAYS);
    if (fres == FR_OK) {
        fres = f_write(&file, text, strlen(text), &bytes_written);
        f_close(&file);
        if (fres == FR_OK) {
            UART_Printf("Wrote %u bytes to '%s'\r\n", bytes_written, path);
        } else {
            UART_Printf("Write failed: %d\r\n", fres);
        }
    } else {
        UART_Printf("Cannot create '%s': error %d\r\n", path, fres);
    }

    osSemaphoreRelease(sdCardMutexHandle);
}

void CMD_Mkdir(int argc, char *argv[]) {
    if (argc < 2) { UART_Print("Usage: mkdir <dirname>\r\n"); return; }

    char path[64];
    BuildPath(argv[1], path, sizeof(path));

    osSemaphoreAcquire(sdCardMutexHandle, osWaitForever);
    FRESULT fres = f_mkdir(path);
    osSemaphoreRelease(sdCardMutexHandle);

    if (fres == FR_OK) UART_Printf("Created '%s'\r\n", path);
    else UART_Printf("Failed: error %d\r\n", fres);
}

void CMD_RM(int argc, char *argv[]) {
    if (argc < 2) { UART_Print("Usage: rm <filename>\r\n"); return; }

    char path[64];
    BuildPath(argv[1], path, sizeof(path));

    osSemaphoreAcquire(sdCardMutexHandle, osWaitForever);
    FRESULT fres = f_unlink(path);
    osSemaphoreRelease(sdCardMutexHandle);

    if (fres == FR_OK) UART_Printf("Removed '%s'\r\n", path);
    else UART_Printf("Failed: error %d\r\n", fres);
}

void CMD_Clear(int argc, char *argv[]) {
    (void)argc; (void)argv;
    DisplayMessage_t msg = {.cmd = DISP_CMD_CLEAR, .color = ST7735_BLACK};
    osMessageQueuePut(displayCommandQueueHandle, &msg, 0, 0);
    UART_Print("Display cleared\r\n");
}

void CMD_Text(int argc, char *argv[]) {
    if (argc < 2) {
        UART_Print("Usage: text \"message\"\r\n");
        return;
    }

    DisplayMessage_t msg;
    msg.cmd = DISP_CMD_TEXT;
    msg.x = 0;
    msg.y = 0;
    msg.color = ST7735_WHITE;
    msg.text[0] = '\0';

    char raw[64] = "";
    for (int i = 1; i < argc && strlen(raw) + strlen(argv[i]) + 2 < sizeof(raw); i++) {
        strcat(raw, argv[i]);
        if (i < argc - 1) strcat(raw, " ");
    }

    /* Strip optional surrounding quotes */
    size_t len = strlen(raw);
    if (len >= 2 && raw[0] == '"' && raw[len - 1] == '"') {
        raw[len - 1] = '\0';
        memmove(raw, raw + 1, len - 1);
    }

    strncpy(msg.text, raw, sizeof(msg.text) - 1);
    msg.text[sizeof(msg.text) - 1] = '\0';

    osMessageQueuePut(displayCommandQueueHandle, &msg, 0, 0);
    UART_Print("Text sent\r\n");
}

void CMD_Fill(int argc, char *argv[]) {
    DisplayMessage_t msg = {.cmd = DISP_CMD_FILL_SCREEN, .color = ST7735_BLACK};

    if (argc > 1) {
        if      (strcmp(argv[1], "red")     == 0) msg.color = ST7735_RED;
        else if (strcmp(argv[1], "green")   == 0) msg.color = ST7735_GREEN;
        else if (strcmp(argv[1], "blue")    == 0) msg.color = ST7735_BLUE;
        else if (strcmp(argv[1], "white")   == 0) msg.color = ST7735_WHITE;
        else if (strcmp(argv[1], "yellow")  == 0) msg.color = ST7735_YELLOW;
        else if (strcmp(argv[1], "cyan")    == 0) msg.color = ST7735_CYAN;
        else if (strcmp(argv[1], "magenta") == 0) msg.color = ST7735_MAGENTA;
    }

    osMessageQueuePut(displayCommandQueueHandle, &msg, 0, 0);
    UART_Print("Fill sent\r\n");
}
void CMD_SetFPS(int argc, char *argv[])
{
    if (argc < 2) {
        UART_Printf("Usage: setfps <value>\r\n");
        return;
    }

    int newFps = atoi(argv[1]);   // convert string to int

    DisplayMessage_t msg = {
        .cmd = DISP_CMD_FPS,
        .color = ST7735_RED,
        .fps = newFps
    };

    fps = 1000/msg.fps;

    osMessageQueuePut(displayCommandQueueHandle, &msg, 0, 0);

    UART_Printf("FPS set to %d\r\n", newFps);
}

void CMD_Pendulum(int argc, char *argv[])
{
	DisplayMessage_t msg = {
			.cmd = DISP_CMD_PENDULUM,
	};

	osMessageQueuePut(displayCommandQueueHandle, &msg, 0, 0);
	UART_Printf("Drawing a pendulum \r\n");
}

void CMD_Pattern(int argc, char *argv[])
{
	DisplayMessage_t msg = {
			.cmd = DISP_CMD_PATTERN,
	};

	osMessageQueuePut(displayCommandQueueHandle, &msg, 0, 0);
	//UART_Printf("SDPath = '%s'\r\n", SDPath);
	UART_Printf("Drawing a gradient \r\n");
}

void CMD_Home(int argc, char *argv[]) {
    (void)argc; (void)argv;
    DisplayMessage_t msg = {.cmd = DISP_CMD_MAIN, .color = ST7735_BLACK};
    osMessageQueuePut(displayCommandQueueHandle, &msg, 0, 0);
    UART_Print("Main display activated\r\n");
}

void CMD_Info(int argc, char *argv[]) {
    (void)argc; (void)argv;
    osSemaphoreAcquire(sdCardMutexHandle, osWaitForever);

    UART_Print("\r\n=== SD Card Information ===\r\n");
    UART_Printf("Initialized: %s\r\n", sd_card.initialized ? "Yes" : "No");
    UART_Printf("Card Type: 0x%02X", sd_card.card_type);
    if (sd_card.card_type & CT_SD2)   UART_Print(" SDv2");
    if (sd_card.card_type & CT_SD1)   UART_Print(" SDv1");
    if (sd_card.card_type & CT_MMC)   UART_Print(" MMC");
    if (sd_card.card_type & CT_BLOCK) UART_Print(" SDHC/SDXC");
    UART_Print("\r\n");
    UART_Printf("SPI: SPI1, CS=PC11, SCK=PA5, MOSI=PA7, MISO=PG9\r\n");

    if (sd_card.initialized) {
        uint32_t sectors = 0;
        if (SD_GetCardInfo(&sd_card, &sectors) == SD_OK && sectors > 0) {
            UART_Printf("Capacity: ~%lu MB\r\n", sectors / 2 / 1024);
        }

        DWORD fre_clust, fre_sect, tot_sect;
        FATFS *fs_ptr = &fs;
        FRESULT fres = f_getfree(SDPath, &fre_clust, &fs_ptr);
        if (fres == FR_OK) {
            tot_sect = (fs.n_fatent - 2) * fs.csize;
            fre_sect = fre_clust * fs.csize;
            UART_Printf("FAT Total: %lu KB\r\n", tot_sect / 2);
            UART_Printf("FAT Free:  %lu KB\r\n", fre_sect / 2);
        }
    }

    UART_Print("===========================\r\n");
    osSemaphoreRelease(sdCardMutexHandle);
}

void CMD_Uptime(int argc, char *argv[]) {
    (void)argc; (void)argv;
    uint32_t seconds = HAL_GetTick() / 1000U;
    UART_Printf("Uptime: %lu:%02lu\r\n",
                (unsigned long)(seconds / 60U),
                (unsigned long)(seconds % 60U));
}

void CMD_Format(int argc, char *argv[]) {
    (void)argc; (void)argv;
    UART_Print("Format not implemented for safety. Use a PC to format as FAT32.\r\n");
}

void CMD_Debug(int argc, char *argv[]) {
    CMD_CardState(argc, argv);
}

void CMD_CardState(int argc, char *argv[]) {
    (void)argc; (void)argv;
    UART_Print("\r\n=== SD Card State ===\r\n");
    UART_Printf("Initialized: %s\r\n", sd_card.initialized ? "YES" : "NO");
    UART_Printf("Card Type:   0x%02X", sd_card.card_type);
    if (sd_card.card_type & CT_SD2)   UART_Print(" SDv2");
    if (sd_card.card_type & CT_SD1)   UART_Print(" SDv1");
    if (sd_card.card_type & CT_MMC)   UART_Print(" MMC");
    if (sd_card.card_type & CT_BLOCK) UART_Print(" BlockAddr(SDHC)");
    UART_Print("\r\n");
    UART_Printf("SPI Mode:    Mode 0 (CPOL=0, CPHA=0)\r\n");
    UART_Printf("SPI Pins:    SCK=PA5  MOSI=PA7  MISO=PG9  CS=PC11\r\n");
    GPIO_PinState cs = HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_11);
    UART_Printf("CS Pin:      %s\r\n", cs == GPIO_PIN_SET ? "HIGH (idle)" : "LOW (active)");
    UART_Print("=====================\r\n");
}

void CMD_Reinit(int argc, char *argv[]) {
    (void)argc; (void)argv;
    UART_Print("\r\nRe-initializing SD card...\r\n");

    osSemaphoreAcquire(sdCardMutexHandle, osWaitForever);

    sd_card.initialized = false;
    sd_card.card_type = 0;

    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_11, GPIO_PIN_SET);
    osDelay(500);

    if (SD_Init(&sd_card) == SD_OK) {
        UART_Printf("Re-init OK! Card type: 0x%02X\r\n", sd_card.card_type);

        FRESULT fres = f_mount(&fs, SDPath, 1);
        if (fres == FR_OK) {
            UART_Print("Filesystem re-mounted!\r\n");
        } else {
            UART_Printf("Mount failed: %d\r\n", fres);
        }
    } else {
        UART_Print("Re-init FAILED!\r\n");
        UART_Print("Check wiring and card insertion.\r\n");
    }

    osSemaphoreRelease(sdCardMutexHandle);
}

void CMD_TestMount(int argc, char *argv[]) {
    (void)argc; (void)argv;
    UART_Print("\r\n=== Mount Test ===\r\n");

    if (!sd_card.initialized) {
        UART_Print("Card not initialized. Run 'reinit' first.\r\n");
        return;
    }

    osSemaphoreAcquire(sdCardMutexHandle, osWaitForever);

    FRESULT fres = f_mount(&fs, SDPath, 1);
    UART_Printf("f_mount result: %d ", fres);
    switch (fres) {
        case FR_OK:            UART_Print("(SUCCESS)\r\n"); break;
        case FR_NO_FILESYSTEM: UART_Print("(No FAT - format card as FAT32)\r\n"); break;
        case FR_DISK_ERR:      UART_Print("(Disk I/O error)\r\n"); break;
        case FR_NOT_READY:     UART_Print("(Not ready)\r\n"); break;
        default:               UART_Print("\r\n"); break;
    }

    if (fres == FR_OK) {
        DWORD fre_clust, fre_sect, tot_sect;
        FATFS *fp = &fs;
        if (f_getfree(SDPath, &fre_clust, &fp) == FR_OK) {
            tot_sect = (fs.n_fatent - 2) * fs.csize;
            fre_sect = fre_clust * fs.csize;
            UART_Printf("Total: %lu KB, Free: %lu KB\r\n", tot_sect / 2, fre_sect / 2);
        }
    }

    osSemaphoreRelease(sdCardMutexHandle);
    UART_Print("==================\r\n");
}

void CMD_TestSector0(int argc, char *argv[]) {
    (void)argc; (void)argv;
    uint8_t buffer[512];

    UART_Print("\r\n=== Sector 0 Test ===\r\n");

    if (!sd_card.initialized) {
        UART_Print("Card not initialized. Run 'reinit' first.\r\n");
        return;
    }

    osSemaphoreAcquire(sdCardMutexHandle, osWaitForever);
    SD_StatusTypeDef result = SD_ReadSingleBlock(&sd_card, 0, buffer);
    osSemaphoreRelease(sdCardMutexHandle);

    if (result != SD_OK) {
        UART_Printf("Read failed: error %d\r\n", result);
        return;
    }

    UART_Print("First 64 bytes:\r\n");
    for (int i = 0; i < 64; i++) {
        UART_Printf("%02X ", buffer[i]);
        if ((i + 1) % 16 == 0) UART_Print("\r\n");
    }

    UART_Printf("\r\nBoot signature: 0x%02X 0x%02X", buffer[510], buffer[511]);
    if (buffer[510] == 0x55 && buffer[511] == 0xAA) {
        UART_Print(" (VALID)\r\n");
        if (buffer[54] == 'F' && buffer[55] == 'A' && buffer[56] == 'T')
            UART_Print("FAT signature at offset 54 found!\r\n");
        if (buffer[82] == 'F' && buffer[83] == 'A' && buffer[84] == 'T')
            UART_Print("FAT32 signature at offset 82 found!\r\n");
    } else {
        UART_Print(" (INVALID - card not formatted)\r\n");
    }
    UART_Print("====================\r\n");
}

void CMD_TestMultiRead(int argc, char *argv[]) {
    (void)argc; (void)argv;
    uint8_t buffer[512];

    UART_Print("\r\n=== Multi-Sector Read Test ===\r\n");

    if (!sd_card.initialized) {
        UART_Print("Card not initialized. Run 'reinit' first.\r\n");
        return;
    }

    for (uint32_t sector = 0; sector < 3; sector++) {
        osSemaphoreAcquire(sdCardMutexHandle, osWaitForever);
        SD_StatusTypeDef result = SD_ReadSingleBlock(&sd_card, sector, buffer);
        osSemaphoreRelease(sdCardMutexHandle);

        UART_Printf("Sector %lu: ", sector);
        if (result == SD_OK) {
            UART_Print("OK  [");
            for (int i = 0; i < 8; i++) UART_Printf("%02X ", buffer[i]);
            UART_Print("...]\r\n");
        } else {
            UART_Printf("FAILED (error %d)\r\n", result);
            break;
        }
    }
    UART_Print("==============================\r\n");
}
