#ifndef BRIDGE_H
#define BRIDGE_H
#include <stdint.h>
#include <stdbool.h>
#include "app_config.h"

typedef struct { uint32_t ts, id; uint8_t ext, rtr, dlc, data[8]; } can_frame_t;
typedef struct { uint32_t ts; uint16_t len; uint8_t data[RS485_RX_BUF]; } rs485_frame_t;
typedef struct { uint32_t can_rx, can_drop, can_err, can_tx, rs_frames, rs_bytes, rs_drop, rs_err; } bridge_stats_t;

void     bridge_init(void);
bool     bridge_can_pop(can_frame_t *f);
uint16_t bridge_can_pending(void);
bool     bridge_rs485_pop(rs485_frame_t *f);
int      bridge_can_send(uint32_t id, bool ext, bool rtr, const uint8_t *d, uint8_t dlc);
int      bridge_can_set(uint32_t mode, uint32_t kbps);   /* 0 = ok */
uint32_t bridge_can_kbps(void);
uint32_t bridge_can_mode(void);
const char *bridge_can_mode_str(void);
int      bridge_rs485_send(const uint8_t *d, uint16_t n);
int      bridge_rs485_set_baud(uint32_t baud);
const bridge_stats_t *bridge_stats(void);
#endif
