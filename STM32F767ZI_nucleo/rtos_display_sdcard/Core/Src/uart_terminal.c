/**
 * @file uart_terminal.c
 * @brief Simple UART command terminal implementation
 */

#include "uart_terminal.h"
#include "cmsis_os.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>

/* Global terminal instance */
static UART_Terminal_t *g_terminal = NULL;

/* Command registry */
static const UART_Command_t *g_commands = NULL;
static uint8_t g_command_count = 0;

/* Synchronization object to move command processing to task context */
static osSemaphoreId_t uartRxSemHandle = NULL;

/* Internal functions */
static void UART_Terminal_ParseCommand(UART_Terminal_t *term);
static void UART_Terminal_ExecuteCommand(char *cmd_line);
static void UART_Terminal_ShowHelp(void);

/**
 * @brief Initialize UART terminal
 */
void UART_Terminal_Init(UART_Terminal_t *term, UART_HandleTypeDef *huart) {
    term->huart = huart;
    term->rx_index = 0;
    term->cmd_ready = false;
    memset(term->rx_buffer, 0, UART_RX_BUFFER_SIZE);
    memset(term->cmd_buffer, 0, UART_CMD_BUFFER_SIZE);
    
    g_terminal = term;
    
    /* Create semaphore used to signal incoming characters from ISR to task */
    if (uartRxSemHandle == NULL) {
        const osSemaphoreAttr_t semAttr = { .name = "uartRxSem" };
        uartRxSemHandle = osSemaphoreNew(1, 0, &semAttr);
    }
    
    /* Print welcome message */
    UART_Terminal_Print(term, "\r\n");
    UART_Terminal_Print(term, "========================================\r\n");
    UART_Terminal_Print(term, "  STM32F767 SD Card Terminal\r\n");
    UART_Terminal_Print(term, "  Type 'help' for available commands\r\n");
    UART_Terminal_Print(term, "========================================\r\n");
    UART_Terminal_Print(term, "> ");
    
    /* Start receiving in interrupt mode */
    HAL_UART_Receive_IT(term->huart, (uint8_t*)&term->rx_buffer[term->rx_index], 1);
}

/**
 * @brief Register command table
 */
void UART_Terminal_RegisterCommands(const UART_Command_t *commands, uint8_t count) {
    g_commands = commands;
    g_command_count = count;
}

/**
 * @brief Process incoming characters (call from UART RX callback)
 */
void UART_Terminal_Process(UART_Terminal_t *term) {
    /* This is called from HAL_UART_RxCpltCallback */
    char received = term->rx_buffer[term->rx_index];
    
    /* Echo character */
    HAL_UART_Transmit(term->huart, (uint8_t*)&received, 1, 100);
    
    if (received == '\r' || received == '\n') {
        /* Command complete */
        term->rx_buffer[term->rx_index] = '\0';
        
        if (term->rx_index > 0) {
            /* Copy to command buffer */
            strncpy(term->cmd_buffer, term->rx_buffer, UART_CMD_BUFFER_SIZE - 1);
            term->cmd_ready = true;
        }
        
        term->rx_index = 0;
        UART_Terminal_Print(term, "\r\n");
        
        /* Process command if ready */
        if (term->cmd_ready) {
            UART_Terminal_ExecuteCommand(term->cmd_buffer);
            term->cmd_ready = false;
        }
        
        UART_Terminal_Print(term, "> ");
    } 
    else if (received == '\b' || received == 127) {
        /* Backspace */
        if (term->rx_index > 0) {
            term->rx_index--;
            UART_Terminal_Print(term, " \b");  /* Erase character */
        }
    }
    else if (received >= 32 && received <= 126) {
        /* Printable character */
        if (term->rx_index < UART_RX_BUFFER_SIZE - 1) {
            term->rx_buffer[term->rx_index++] = received;
        }
    }
    
    /* Continue receiving */
    HAL_UART_Receive_IT(term->huart, (uint8_t*)&term->rx_buffer[term->rx_index], 1);
}

/**
 * @brief Execute parsed command
 */
static void UART_Terminal_ExecuteCommand(char *cmd_line) {
    char *argv[UART_MAX_ARGS];
    int argc = 0;
    
    /* Parse command line into arguments */
    char *token = strtok(cmd_line, " ");
    while (token != NULL && argc < UART_MAX_ARGS) {
        argv[argc++] = token;
        token = strtok(NULL, " ");
    }
    
    if (argc == 0) {
        return;
    }
    
    /* Check for built-in help command */
    if (strcmp(argv[0], "help") == 0) {
        UART_Terminal_ShowHelp();
        return;
    }
    
    /* Search for command in registry */
    for (uint8_t i = 0; i < g_command_count; i++) {
        if (strcmp(argv[0], g_commands[i].name) == 0) {
            /* Execute command handler */
            g_commands[i].handler(argc, argv);
            return;
        }
    }
    
    /* Command not found */
    UART_Printf("Unknown command: %s\r\n", argv[0]);
    UART_Print("Type 'help' for available commands\r\n");
}

/**
 * @brief Show help message
 */
static void UART_Terminal_ShowHelp(void) {
    UART_Print("\r\nAvailable commands:\r\n");
    UART_Print("  help - Show this help message\r\n");
    
    for (uint8_t i = 0; i < g_command_count; i++) {
        UART_Printf("  %s - %s\r\n", g_commands[i].name, g_commands[i].help);
    }
    UART_Print("\r\n");
}

/**
 * @brief Print string to UART
 */
void UART_Terminal_Print(UART_Terminal_t *term, const char *str) {
    HAL_UART_Transmit(term->huart, (uint8_t*)str, strlen(str), 1000);
}

/**
 * @brief Printf-style output to UART
 */
void UART_Terminal_Printf(UART_Terminal_t *term, const char *format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    UART_Terminal_Print(term, buffer);
}

/**
 * @brief Global print function
 */
void UART_Print(const char *str) {
    if (g_terminal) {
        UART_Terminal_Print(g_terminal, str);
    }
}

/**
 * @brief Global printf function
 */
void UART_Printf(const char *format, ...) {
    if (g_terminal) {
        char buffer[256];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);
        UART_Terminal_Print(g_terminal, buffer);
    }
}

/**
 * @brief UART RX complete callback (weak override)
 */
void UART_Terminal_TaskLoop(void) {
    for (;;) {
        if (uartRxSemHandle != NULL) {
            osSemaphoreAcquire(uartRxSemHandle, osWaitForever);
            if (g_terminal) {
                UART_Terminal_Process(g_terminal);
            }
        }
    }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (g_terminal && huart == g_terminal->huart && uartRxSemHandle != NULL) {
        osSemaphoreRelease(uartRxSemHandle);
    }
}
