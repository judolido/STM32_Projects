#ifndef APP_H
#define APP_H
#include <stdint.h>

void app_init(void);
void app_poll(void);
void app_reply(const char *fmt, ...);
void app_publish_info(void);
void app_publish_stats(void);
int  app_udp_send(const uint8_t ip[4], uint16_t port, const uint8_t *d, uint16_t n);
extern volatile uint8_t app_mon_can, app_mon_rs485;
#endif
