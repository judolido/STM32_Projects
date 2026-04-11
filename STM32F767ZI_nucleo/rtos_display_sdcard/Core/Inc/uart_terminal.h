/**
 * @file uart_terminal.h
 * @brief Simple UART command terminal for SD card and display control
 */

#ifndef UART_TERMINAL_H
#define UART_TERMINAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f7xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

/* Configuration */
#define UART_RX_BUFFER_SIZE     256
#define UART_CMD_BUFFER_SIZE    128
#define UART_MAX_ARGS           8

/* Terminal structure */
typedef struct {
    UART_HandleTypeDef *huart;
    char rx_buffer[UART_RX_BUFFER_SIZE];
    uint16_t rx_index;
    char cmd_buffer[UART_CMD_BUFFER_SIZE];
    bool cmd_ready;
} UART_Terminal_t;

/* Command handler function pointer */
typedef void (*UART_CommandHandler)(int argc, char *argv[]);

/* Command structure */
typedef struct {
    const char *name;
    const char *help;
    UART_CommandHandler handler;
} UART_Command_t;

/* Public functions */
void UART_Terminal_Init(UART_Terminal_t *term, UART_HandleTypeDef *huart);
void UART_Terminal_Process(UART_Terminal_t *term);
void UART_Terminal_TaskLoop(void);
void UART_Terminal_Print(UART_Terminal_t *term, const char *str);
void UART_Terminal_Printf(UART_Terminal_t *term, const char *format, ...);
void UART_Terminal_RegisterCommands(const UART_Command_t *commands, uint8_t count);

/* Utility functions */
void UART_Print(const char *str);
void UART_Printf(const char *format, ...);

#ifdef __cplusplus
}
#endif

#endif /* UART_TERMINAL_H */
