#ifndef MQTT_MIN_H
#define MQTT_MIN_H
#include <stdint.h>
#include <stdbool.h>

#define MQTT_MAX_SUBS 4
typedef void (*mqtt_msg_cb)(const char *topic, const uint8_t *payload, uint16_t len);

void mqtt_init(uint8_t sock, const uint8_t ip[4], uint16_t port, const char *client_id,
               const char *will_topic, const char *will_msg,
               const char *const *subs, uint8_t nsubs, mqtt_msg_cb cb);
void mqtt_task(void);                 /* call every main-loop pass */
bool mqtt_connected(void);
int  mqtt_publish(const char *topic, const uint8_t *payload, uint16_t len, bool retain);
uint32_t mqtt_reconnects(void);
#endif
