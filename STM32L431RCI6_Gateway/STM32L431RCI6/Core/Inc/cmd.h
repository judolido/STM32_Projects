#ifndef CMD_H
#define CMD_H
#include <stdint.h>
void cmd_handle(const char *payload, uint16_t len);   /* parse one text command */
void cmd_poll(uint32_t now_ms);                        /* LED blinking */
#endif
