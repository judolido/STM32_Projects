/* Minimal MQTT 3.1.1 client (QoS0 publish/subscribe) on WIZnet ioLibrary sockets. */
#include "mqtt_min.h"
#include "app_config.h"
#include "main.h"
#include "wizchip_conf.h"
#include "socket.h"
#include "w5500.h"
#include <string.h>
#include <stdio.h>

#define DBG(...) do { if (APP_DEBUG) printf(__VA_ARGS__); } while (0)
#define MQTT_BUF 1024

enum { ST_CLOSED, ST_CONNECTING, ST_WAIT_CONNACK, ST_UP };

static struct {
    uint8_t sock;
    uint8_t ip[4];
    uint16_t port;
    const char *cid;
    const char *will_topic;
    const char *will_msg;
    const char *subs[MQTT_MAX_SUBS];
    uint8_t nsubs;
    mqtt_msg_cb cb;
    uint8_t st;
    uint32_t t_state, t_tx, t_rx, t_ping, reconnects;
    uint16_t rxn, pid;
    uint8_t txb[MQTT_BUF], rxb[MQTT_BUF];
} m;

static uint8_t *put_u16(uint8_t *p, uint16_t v) { *p++ = v >> 8; *p++ = v & 0xFF; return p; }
static uint8_t *put_str(uint8_t *p, const char *s) {
    uint16_t n = (uint16_t)strlen(s); p = put_u16(p, n); memcpy(p, s, n); return p + n;
}
static uint8_t *put_rl(uint8_t *p, uint32_t len) {
    do { uint8_t d = len % 128; len /= 128; if (len) d |= 0x80; *p++ = d; } while (len);
    return p;
}

static void drop_(int line) {
    DBG("mqtt: drop (mqtt_min.c line %d, state %d)\r\n", line, m.st);
    close(m.sock);
    m.st = ST_CLOSED; m.rxn = 0; m.t_state = HAL_GetTick(); m.reconnects++;
}
#define drop() drop_(__LINE__)

static int send_all(const uint8_t *b, uint16_t n) {
    uint32_t t0 = HAL_GetTick();
    while (n) {
        if (getSn_SR(m.sock) != SOCK_ESTABLISHED) return -1;
        uint16_t fsr = getSn_TX_FSR(m.sock);
        if (fsr == 0 || fsr > 2048) { if (HAL_GetTick() - t0 > 1000) return -1; continue; }
        if (fsr > n) fsr = n;
        int32_t r = send(m.sock, (uint8_t *)b, fsr);
        if (r > 0) {
            b += r;
            n -= (uint16_t)r;
            t0 = HAL_GetTick();
            m.t_rx = t0; /* Outbound transmissions refresh link status timestamps */
        }
        else if (r < 0) return -1;
        else if (HAL_GetTick() - t0 > 1000) return -1;
    }
    m.t_tx = HAL_GetTick();
    return 0;
}

static int send_connect(void) {
    uint8_t b[256], *p = b;
    p = put_str(p, "MQTT"); *p++ = 4;
    *p++ = 0x02 | 0x04 | 0x20;                 /* clean session, will, will-retain */
    p = put_u16(p, MQTT_KEEPALIVE_S);
    p = put_str(p, m.cid);
    p = put_str(p, m.will_topic);
    p = put_str(p, m.will_msg);
    uint8_t *q = m.txb; *q++ = 0x10; q = put_rl(q, (uint32_t)(p - b));
    memcpy(q, b, (size_t)(p - b)); q += (p - b);
    return send_all(m.txb, (uint16_t)(q - m.txb));
}

static void send_subscribes(void) {
    for (uint8_t i = 0; i < m.nsubs; i++) {
        uint8_t b[128], *p = b;
        p = put_u16(p, ++m.pid); p = put_str(p, m.subs[i]); *p++ = 0;
        uint8_t *q = m.txb; *q++ = 0x82; q = put_rl(q, (uint32_t)(p - b));
        memcpy(q, b, (size_t)(p - b)); q += (p - b);
        if (send_all(m.txb, (uint16_t)(q - m.txb)) < 0) { drop(); return; }
    }
}

static void handle_packet(uint8_t hdr, const uint8_t *body, uint32_t len) {
    uint32_t now = HAL_GetTick();
    switch (hdr >> 4) {
    case 2:  /* CONNACK */
        if (m.st == ST_WAIT_CONNACK) {
            // FIX: Correctly check the data inside the body array instead of the body pointer address
            if (len >= 2 && body[1] == 0) {
                DBG("mqtt: CONNACK ok\r\n");
                m.st = ST_UP;
                m.t_rx = now;
                m.t_tx = now;
                m.t_ping = now;
                send_subscribes();
            } else { DBG("mqtt: CONNACK refused rc=%d\r\n", len >= 2 ? body[1] : -1); drop(); }
        }
        break;
    case 3: { /* PUBLISH */
        static char topic[96];
        uint8_t qos = (hdr >> 1) & 3;
        if (len < 2) break;
        uint16_t tl = (uint16_t)((body[0] << 8) | body[1]);
        if (2u + tl > len || tl >= sizeof topic) break;
        uint32_t off = 2u + tl;
        if (qos) off += 2;
        if (off > len) break;
        memcpy(topic, body + 2, tl); topic[tl] = 0;
        if (m.cb) m.cb(topic, body + off, (uint16_t)(len - off));
        break; }
    case 9:  /* SUBACK */
        DBG("mqtt: SUBACK received (len %lu)\r\n", (unsigned long)len);
        break;
    case 13: /* PINGRESP */
        DBG("mqtt: PINGRESP received\r\n");
        break;
    default: break;
    }
}

static void pump_rx(void) {
    uint16_t avail = getSn_RX_RSR(m.sock);
    if (avail) {
        uint16_t room = MQTT_BUF - m.rxn;
        if (!room) { drop(); return; }
        if (avail > room) avail = room;
        int32_t r = recv(m.sock, m.rxb + m.rxn, avail);
        if (r <= 0) {
            uint8_t sr = getSn_SR(m.sock);
            DBG("mqtt: recv()=%d rsr=%u sr=0x%02X -> raw read\r\n", (int)r, (unsigned)avail, sr);
            if (sr == SOCK_ESTABLISHED || sr == SOCK_CLOSE_WAIT) {
                wiz_recv_data(m.sock, m.rxb + m.rxn, avail);
                setSn_CR(m.sock, Sn_CR_RECV);
                while (getSn_CR(m.sock));
                r = avail;
            }
        }
        if (r > 0) {
            DBG("mqtt: rx %d bytes:", (int)r);
            for (int k = 0; k < r && k < 8; k++) DBG(" %02X", m.rxb[m.rxn + k]);
            DBG("\r\n");
            m.rxn += (uint16_t)r;
        }
    }

    while (m.rxn >= 2) {
        uint32_t rl = 0, mult = 1; uint8_t i = 1, d;
        do {
            if (i >= m.rxn) return;
            d = m.rxb[i++]; rl += (d & 127) * mult; mult *= 128;
        } while ((d & 128) && i < 5);
        if (d & 128) { drop(); return; }
        uint32_t total = i + rl;
        if (total > MQTT_BUF) { drop(); return; }
        if (m.rxn < total) return;

        handle_packet(m.rxb[0], m.rxb + i, rl);
        if (m.st == ST_CLOSED) return;

        memmove(m.rxb, m.rxb + total, m.rxn - total);
        m.rxn -= (uint16_t)total;
        m.t_rx = HAL_GetTick();
    }
}

void mqtt_init(uint8_t sock, const uint8_t ip[4], uint16_t port, const char *cid,
               const char *wt, const char *wm, const char *const *subs, uint8_t n, mqtt_msg_cb cb) {
    memset(&m, 0, sizeof m);
    m.sock = sock;
    memcpy(m.ip, ip, 4);
    m.port = port; m.cid = cid;
    m.will_topic = wt; m.will_msg = wm; m.cb = cb;
    for (uint8_t i = 0; i < n && i < MQTT_MAX_SUBS; i++) m.subs[m.nsubs++] = subs[i];
    m.st = ST_CLOSED; m.t_state = HAL_GetTick() - MQTT_RETRY_MS;
    m.reconnects = 0;
}

bool mqtt_connected(void) { return m.st == ST_UP; }
uint32_t mqtt_reconnects(void) { return m.reconnects; }

int mqtt_publish(const char *topic, const uint8_t *payload, uint16_t len, bool retain) {
    if (m.st != ST_UP) return -1;
    uint16_t tl = (uint16_t)strlen(topic);
    uint32_t rl = 2u + tl + len;
    if (rl + 5 > MQTT_BUF) return -2;
    uint8_t *p = m.txb;
    *p++ = 0x30 | (retain ? 1 : 0);
    p = put_rl(p, rl); p = put_u16(p, tl);
    memcpy(p, topic, tl); p += tl; memcpy(p, payload, len); p += len;
    if (send_all(m.txb, (uint16_t)(p - m.txb)) < 0) { drop(); return -3; }
    return 0;
}

void mqtt_task(void) {
    uint32_t now = HAL_GetTick();
    switch (m.st) {
    case ST_CLOSED:
        if (now - m.t_state < MQTT_RETRY_MS) return;
        m.t_state = now;
        if (socket(m.sock, Sn_MR_TCP, (uint16_t)(50000 + (now & 0x3FF)), SF_IO_NONBLOCK) != m.sock) return;
        connect(m.sock, m.ip, m.port);
        m.st = ST_CONNECTING;
        break;
    case ST_CONNECTING: {
        uint8_t s = getSn_SR(m.sock);
        if (s == SOCK_ESTABLISHED) {
            DBG("mqtt: TCP connected\r\n");
            m.rxn = 0;
            if (send_connect() == 0) { DBG("mqtt: CONNECT sent\r\n"); m.st = ST_WAIT_CONNACK; m.t_state = now; }
            else drop();
        } else if (s == SOCK_CLOSED || now - m.t_state > 5000) drop();
        break; }
    case ST_WAIT_CONNACK:
        pump_rx();
        if (m.st == ST_WAIT_CONNACK && now - m.t_state > 3000) {
            DBG("mqtt: no CONNACK: sock_sr=0x%02X rx_rsr=%u rxn=%u\r\n",
                getSn_SR(m.sock), (unsigned)getSn_RX_RSR(m.sock), (unsigned)m.rxn);
            drop();
        }
        break;
    case ST_UP:
        if (getSn_SR(m.sock) != SOCK_ESTABLISHED) { drop(); return; }
        pump_rx();
        if (m.st != ST_UP) return;
        if (now - m.t_ping > (uint32_t)MQTT_KEEPALIVE_S * 500UL) {
            const uint8_t ping[2] = {0xC0, 0x00};
            m.t_ping = now;
            if (send_all(ping, 2) < 0) { drop(); return; }
        }
        //if (now - m.t_rx > (uint32_t)MQTT_KEEPALIVE_S * 1500UL) drop();
        break;
    }
}
