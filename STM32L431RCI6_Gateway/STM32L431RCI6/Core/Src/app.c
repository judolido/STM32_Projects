/* Glue: W5500 + DHCP + MQTT + UDP + periodic publishing. */
#include "app.h"
#include "app_config.h"
#include "mqtt_min.h"
#include "bridge.h"
#include "cmd.h"
#include "main.h"
#include "wizchip_conf.h"
#include "socket.h"
#include "../Drivers/W5500/DHCP/dhcp.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

extern SPI_HandleTypeDef hspi2;
void W5500_Select(void); void W5500_Unselect(void);
uint8_t W5500_ReadByte(void); void W5500_WriteByte(uint8_t wb); void W5500_Reset(void);

#define T_STATUS  TOPIC_BASE "status"
#define T_INFO    TOPIC_BASE "info"
#define T_RESP    TOPIC_BASE "resp"
#define T_STATS   TOPIC_BASE "stats"
#define T_CAN_RX  TOPIC_BASE "can/rx"
#define T_RS_RX   TOPIC_BASE "rs485/rx"
#define T_ETH_RX  TOPIC_BASE "eth/rx"
#define T_BTN     TOPIC_BASE "io/btn"

volatile uint8_t app_mon_can = 1, app_mon_rs485 = 1;

#if USE_DHCP
static uint8_t dhcp_buf[1024];
#endif
static char pb[900];                         /* shared publish buffer (static: keeps stack small) */
static bool net_up, was_up, was_net;
static uint32_t t_dhcp, t_stats, t_can, t_btn;
static uint8_t btn_last[3];
static const char *const subs[] = { TOPIC_BASE "cmd", TOPIC_ALL_CMD };

static void spi_wb(uint8_t *b, uint16_t n) { HAL_SPI_Transmit(&hspi2, b, n, HAL_MAX_DELAY); }
static void spi_rb(uint8_t *b, uint16_t n) { HAL_SPI_Receive(&hspi2, b, n, HAL_MAX_DELAY); }

static void fatal_blink(void) { for (;;) { HAL_GPIO_TogglePin(LED1_GPIO_Port, LED1_Pin); HAL_Delay(100); } }

static void on_msg(const char *topic, const uint8_t *p, uint16_t n) { (void)topic; cmd_handle((const char *)p, n); }

void app_reply(const char *fmt, ...) {
    static char b[256]; va_list a;
    va_start(a, fmt); int n = vsnprintf(b, sizeof b, fmt, a); va_end(a);
    if (n > (int)sizeof b - 1) n = sizeof b - 1;
    mqtt_publish(T_RESP, (uint8_t *)b, (uint16_t)n, false);
}

void app_publish_info(void) {
    uint8_t ip[4]; getSIPR(ip);
    int n = snprintf(pb, sizeof pb,
        "{\"dev\":\"%s\",\"fw\":\"%s\",\"ip\":\"%u.%u.%u.%u\",\"up_s\":%lu,\"can_kbps\":%lu,\"can_mode\":\"%s\"}",
        DEVICE_NAME, FW_VERSION, ip[0], ip[1], ip[2], ip[3],
        (unsigned long)(HAL_GetTick() / 1000), (unsigned long)bridge_can_kbps(), bridge_can_mode_str());
    mqtt_publish(T_INFO, (uint8_t *)pb, (uint16_t)n, true);
}

void app_publish_stats(void) {
    const bridge_stats_t *s = bridge_stats();
    int n = snprintf(pb, sizeof pb,
        "{\"up_s\":%lu,\"can_rx\":%lu,\"can_tx\":%lu,\"can_drop\":%lu,\"can_err\":%lu,"
        "\"rs_frames\":%lu,\"rs_bytes\":%lu,\"rs_drop\":%lu,\"rs_err\":%lu,\"mqtt_reconn\":%lu}",
        (unsigned long)(HAL_GetTick() / 1000), (unsigned long)s->can_rx, (unsigned long)s->can_tx,
        (unsigned long)s->can_drop, (unsigned long)s->can_err, (unsigned long)s->rs_frames,
        (unsigned long)s->rs_bytes, (unsigned long)s->rs_drop, (unsigned long)s->rs_err,
        (unsigned long)mqtt_reconnects());
    mqtt_publish(T_STATS, (uint8_t *)pb, (uint16_t)n, false);
}

/* ---------------- UDP (chip-to-chip / laptop datagrams) ---------------- */
int app_udp_send(const uint8_t ip[4], uint16_t port, const uint8_t *d, uint16_t n) {
    if (getSn_SR(SOCK_UDP) != SOCK_UDP) return -100;
    return (int)sendto(SOCK_UDP, (uint8_t *)d, n, (uint8_t *)ip, port);   /* may block on ARP/timeout */
}

static int json_str(char *o, int max, const uint8_t *s, int n) {
    int k = 0;
    for (int i = 0; i < n && k < max - 7; i++) {
        uint8_t c = s[i];
        if (c == '"' || c == '\\') { o[k++] = '\\'; o[k++] = (char)c; }
        else if (c >= 32 && c < 127) o[k++] = (char)c;
        else k += snprintf(o + k, max - k, "\\u%04X", c);
    }
    return k;
}

static void udp_task(void) {
    static uint8_t b[256];
    if (getSn_SR(SOCK_UDP) != SOCK_UDP) { socket(SOCK_UDP, Sn_MR_UDP, UDP_PORT, SF_IO_NONBLOCK); return; }
    if (!getSn_RX_RSR(SOCK_UDP)) return;
    uint8_t ip[4]; uint16_t port;
    int32_t r = recvfrom(SOCK_UDP, b, sizeof b - 1, ip, &port);
    if (r <= 0) return;
    b[r] = 0;
    if (b[0] == '!') { cmd_handle((char *)b + 1, (uint16_t)(r - 1)); return; }   /* remote command */
    if (!mqtt_connected()) return;
    int n = snprintf(pb, sizeof pb, "{\"src\":\"%u.%u.%u.%u\",\"port\":%u,\"s\":\"", ip[0], ip[1], ip[2], ip[3], port);
    n += json_str(pb + n, (int)sizeof pb - n - 4, b, r);
    pb[n++] = '"'; pb[n++] = '}';
    mqtt_publish(T_ETH_RX, (uint8_t *)pb, (uint16_t)n, false);
}

/* ---------------- Publishing of captured traffic ---------------- */
static int put_hex(char *o, const uint8_t *d, int n) {
    static const char H[] = "0123456789ABCDEF"; int k = 0;
    for (int i = 0; i < n; i++) { o[k++] = H[d[i] >> 4]; o[k++] = H[d[i] & 15]; }
    return k;
}

static void flush_can(void) {
    can_frame_t f; int n = 0, c = 0;
    if (!app_mon_can) { while (bridge_can_pop(&f)); return; }
    pb[n++] = '[';
    while (c < CAN_BATCH_MAX && n < (int)sizeof pb - 130 && bridge_can_pop(&f)) {
        n += snprintf(pb + n, sizeof pb - n, "%s{\"t\":%lu,\"id\":\"%lX\",\"x\":%u,\"r\":%u,\"l\":%u,\"d\":\"",
                      c ? "," : "", (unsigned long)f.ts, (unsigned long)f.id,
                      (unsigned)f.ext, (unsigned)f.rtr, (unsigned)f.dlc);
        n += put_hex(pb + n, f.data, f.rtr ? 0 : f.dlc);
        pb[n++] = '"'; pb[n++] = '}'; c++;
    }
    if (c) { pb[n++] = ']'; mqtt_publish(T_CAN_RX, (uint8_t *)pb, (uint16_t)n, false); }
}

static void flush_rs485(void) {
    rs485_frame_t f; int k = 0;
    while (k < 4 && bridge_rs485_pop(&f)) {
        if (!app_mon_rs485) continue;
        int n = snprintf(pb, sizeof pb, "{\"t\":%lu,\"n\":%u,\"d\":\"", (unsigned long)f.ts, (unsigned)f.len);
        n += put_hex(pb + n, f.data, f.len);
        pb[n++] = '"'; pb[n++] = '}';
        mqtt_publish(T_RS_RX, (uint8_t *)pb, (uint16_t)n, false); k++;
    }
}

static const struct { GPIO_TypeDef *p; uint16_t pin; } btn[3] = {
    {BTN1_GPIO_Port, BTN1_Pin}, {BTN2_GPIO_Port, BTN2_Pin}, {BTN3_GPIO_Port, BTN3_Pin}
};
static void buttons_poll(uint32_t now) {
    if (now - t_btn < 20) return;            /* 20 ms sampling doubles as debounce */
    t_btn = now;
    for (int i = 0; i < 3; i++) {
        uint8_t v = HAL_GPIO_ReadPin(btn[i].p, btn[i].pin) == GPIO_PIN_SET;
        if (v != btn_last[i]) {
            btn_last[i] = v;
            int n = snprintf(pb, sizeof pb, "{\"n\":%d,\"v\":%u}", i + 1, v);
            mqtt_publish(T_BTN, (uint8_t *)pb, (uint16_t)n, false);
        }
    }
}

/* ---------------- Init / main-loop poll ---------------- */
void app_init(void) {
    W5500_Reset();
    reg_wizchip_cs_cbfunc(W5500_Select, W5500_Unselect);
    reg_wizchip_spi_cbfunc(W5500_ReadByte, W5500_WriteByte);
    reg_wizchip_spiburst_cbfunc(spi_rb, spi_wb);
    uint8_t sz[8] = {2, 2, 2, 2, 2, 2, 2, 2};
    if (wizchip_init(sz, sz) < 0) fatal_blink();
    if (getVERSIONR() != 0x04) fatal_blink();            /* W5500 not answering on SPI */

    wiz_NetInfo ni = { .mac = MAC_ADDR, .ip = NET_IP, .sn = NET_SN, .gw = NET_GW, .dns = {8,8,8,8},
#if USE_DHCP
                       .dhcp = NETINFO_DHCP };
#else
                       .dhcp = NETINFO_STATIC };
#endif
    ctlnetwork(CN_SET_NETINFO, (void *)&ni);
    setSHAR(ni.mac);
#if USE_DHCP
    DHCP_init(SOCK_DHCP, dhcp_buf);
#endif

    bridge_init();
    for (int i = 0; i < 3; i++) btn_last[i] = HAL_GPIO_ReadPin(btn[i].p, btn[i].pin) == GPIO_PIN_SET;

    static const uint8_t bip[4] = BROKER_IP;
    mqtt_init(SOCK_MQTT, bip, BROKER_PORT, DEVICE_NAME, T_STATUS, "offline", subs, 2, on_msg);
}

void app_poll(void) {
    uint32_t now = HAL_GetTick();

#if USE_DHCP
    if (now - t_dhcp >= 1000) { t_dhcp = now; DHCP_time_handler(); }
    if (DHCP_run() == DHCP_FAILED) DHCP_init(SOCK_DHCP, dhcp_buf);
#endif
    uint8_t ip[4]; getSIPR(ip);
    net_up = (ip[0] | ip[1] | ip[2] | ip[3]) && wizphy_getphylink() == PHY_LINK_ON;

    if (net_up && !was_net)
        printf("net: IP %u.%u.%u.%u, broker %u.%u.%u.%u:%d\r\n", ip[0], ip[1], ip[2], ip[3],
               (unsigned)BROKER_IP_0, (unsigned)BROKER_IP_1, (unsigned)BROKER_IP_2, (unsigned)BROKER_IP_3, BROKER_PORT);
    if (!net_up && was_net) printf("net: down\r\n");
    was_net = net_up;
    if (net_up) { mqtt_task(); udp_task(); }

    bool up = mqtt_connected();
    if (up && !was_up) {
        printf("mqtt: up, publishing online\r\n");
        mqtt_publish(T_STATUS, (const uint8_t *)"online", 6, true);
        app_publish_info();
    }
    was_up = up;

    if (up) {
        if (now - t_can >= CAN_BATCH_MS || bridge_can_pending() >= CAN_BATCH_MAX) { t_can = now; flush_can(); }
        flush_rs485();
        if (now - t_stats >= 5000) { t_stats = now; app_publish_stats(); }
        buttons_poll(now);
    }
    cmd_poll(now);
}
