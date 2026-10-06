/* Text command interpreter. Commands arrive on site/<dev>/cmd or site/all/cmd (MQTT),
   or via UDP datagrams starting with '!'. Replies go to site/<dev>/resp. */
#include "cmd.h"
#include "app.h"
#include "bridge.h"
#include "main.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CMD_MAX 256

static const struct { GPIO_TypeDef *port; uint16_t pin; } ledp[3] = {
    {LED1_GPIO_Port, LED1_Pin}, {LED2_GPIO_Port, LED2_Pin}, {LED3_GPIO_Port, LED3_Pin}
};
static struct { uint8_t mode; uint16_t period; uint32_t last; } led[3];   /* mode 0 manual, 2 blink */

void cmd_poll(uint32_t now) {
    for (int i = 0; i < 3; i++)
        if (led[i].mode == 2 && now - led[i].last >= led[i].period) {
            led[i].last = now; HAL_GPIO_TogglePin(ledp[i].port, ledp[i].pin);
        }
}

static int split(char *s, char **v, int max) {
    int n = 0;
    while (*s && n < max) {
        while (*s == ' ') s++;
        if (!*s) break;
        v[n++] = s;
        if (n == max) break;                 /* last arg takes the rest of the line */
        while (*s && *s != ' ') s++;
        if (*s) *s++ = 0;
    }
    return n;
}
static int hv(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
static int hex2bin(const char *s, uint8_t *o, int max) {
    int n = 0;
    if (!strcmp(s, "-")) return 0;
    while (s[0] && s[1]) {
        int a = hv(s[0]), b = hv(s[1]);
        if (a < 0 || b < 0 || n >= max) return -1;
        o[n++] = (uint8_t)((a << 4) | b); s += 2;
    }
    return *s ? -1 : n;
}

static const char HELP[] =
    "cmds: ping | info | stats | reboot | led <1-3> on|off|toggle|blink [ms] | "
    "can send <idhex> <datahex|-> [ext|rtr] | can mode normal|silent|loopback|silentloop | can baud <kbps> | "
    "rs485 send <hex> | rs485 text <str> | rs485 baud <n> | eth send <ip> <port> <text> | "
    "mon can|rs485 on|off";

void cmd_handle(const char *p, uint16_t len) {
    static char line[CMD_MAX];
    if (len >= CMD_MAX) len = CMD_MAX - 1;
    memcpy(line, p, len); line[len] = 0;
    while (len && (line[len-1] == '\r' || line[len-1] == '\n' || line[len-1] == ' ')) line[--len] = 0;
    char *v[5]; int n = split(line, v, 5);
    if (!n) return;

    if (!strcmp(v[0], "ping")) { app_reply("pong %lu", (unsigned long)HAL_GetTick()); }
    else if (!strcmp(v[0], "help")) { app_reply("%s", HELP); }
    else if (!strcmp(v[0], "info")) { app_publish_info(); }
    else if (!strcmp(v[0], "stats")) { app_publish_stats(); }
    else if (!strcmp(v[0], "reboot")) { app_reply("ok rebooting"); HAL_Delay(100); NVIC_SystemReset(); }

    else if (!strcmp(v[0], "led") && n >= 3) {
        int i = atoi(v[1]) - 1;
        if (i < 0 || i > 2) { app_reply("err led 1-3"); return; }
        if (!strcmp(v[2], "on"))       { led[i].mode = 0; HAL_GPIO_WritePin(ledp[i].port, ledp[i].pin, GPIO_PIN_SET); }
        else if (!strcmp(v[2], "off")) { led[i].mode = 0; HAL_GPIO_WritePin(ledp[i].port, ledp[i].pin, GPIO_PIN_RESET); }
        else if (!strcmp(v[2], "toggle")) { led[i].mode = 0; HAL_GPIO_TogglePin(ledp[i].port, ledp[i].pin); }
        else if (!strcmp(v[2], "blink")) {
            int ms = n > 3 ? atoi(v[3]) : 500; if (ms < 20) ms = 20;
            led[i].mode = 2; led[i].period = (uint16_t)ms; led[i].last = HAL_GetTick();
        } else { app_reply("err led mode"); return; }
        app_reply("ok led %d %s", i + 1, v[2]);
    }

    else if (!strcmp(v[0], "can") && n >= 2) {
        if (!strcmp(v[1], "send") && n >= 4) {
            uint8_t d[8]; uint32_t id = strtoul(v[2], NULL, 16);
            int dl = hex2bin(v[3], d, 8);
            bool rtr = (n > 4 && !strcmp(v[4], "rtr"));
            bool ext = (n > 4 && !strcmp(v[4], "ext")) || id > 0x7FF;
            if (dl < 0) { app_reply("err bad data hex (max 8 bytes)"); return; }
            int r = bridge_can_send(id, ext, rtr, d, (uint8_t)dl);
            if (r) app_reply("err can send %d (silent mode? bus off? mailboxes full?)", r);
            else   app_reply("ok can tx id=%lX len=%d", (unsigned long)id, dl);
        } else if (!strcmp(v[1], "mode") && n >= 3) {
            uint32_t md;
            if      (!strcmp(v[2], "normal"))     md = CAN_MODE_NORMAL;
            else if (!strcmp(v[2], "silent"))     md = CAN_MODE_SILENT;
            else if (!strcmp(v[2], "loopback"))   md = CAN_MODE_LOOPBACK;
            else if (!strcmp(v[2], "silentloop")) md = CAN_MODE_SILENT_LOOPBACK;
            else { app_reply("err mode"); return; }
            int r = bridge_can_set(md, bridge_can_kbps());
            app_reply(r ? "err can mode %d" : "ok can mode %s", r ? r : 0, v[2]);
        } else if (!strcmp(v[1], "baud") && n >= 3) {
            int r = bridge_can_set(bridge_can_mode(), (uint32_t)atoi(v[2]));
            if (r) app_reply("err can baud %d (use 125/250/500/1000)", r);
            else   app_reply("ok can baud %s kbps", v[2]);
        } else app_reply("err can usage");
    }

    else if (!strcmp(v[0], "rs485") && n >= 3) {
        if (!strcmp(v[1], "send")) {
            static uint8_t b[128]; int k = hex2bin(v[2], b, sizeof b);
            if (k <= 0) { app_reply("err bad hex"); return; }
            app_reply(bridge_rs485_send(b, (uint16_t)k) ? "err rs485 tx" : "ok rs485 tx %d bytes", k);
        } else if (!strcmp(v[1], "text")) {
            uint16_t k = (uint16_t)strlen(v[2]);
            app_reply(bridge_rs485_send((uint8_t *)v[2], k) ? "err rs485 tx" : "ok rs485 tx %d bytes", k);
        } else if (!strcmp(v[1], "baud")) {
            int r = bridge_rs485_set_baud((uint32_t)atoi(v[2]));
            app_reply(r ? "err rs485 baud %d" : "ok rs485 baud %s", r ? r : 0, v[2]);
        } else app_reply("err rs485 usage");
    }

    else if (!strcmp(v[0], "eth") && n >= 5 && !strcmp(v[1], "send")) {
        unsigned a, b, c, d, port;
        if (sscanf(v[2], "%u.%u.%u.%u", &a, &b, &c, &d) != 4) { app_reply("err ip"); return; }
        port = (unsigned)atoi(v[3]);
        uint8_t ip[4] = {(uint8_t)a, (uint8_t)b, (uint8_t)c, (uint8_t)d};
        int r = app_udp_send(ip, (uint16_t)port, (uint8_t *)v[4], (uint16_t)strlen(v[4]));
        app_reply(r > 0 ? "ok udp tx %d bytes" : "err udp tx %d", r > 0 ? r : r);
    }

    else if (!strcmp(v[0], "mon") && n >= 3) {
        uint8_t on = !strcmp(v[2], "on");
        if (!strcmp(v[1], "can")) app_mon_can = on;
        else if (!strcmp(v[1], "rs485")) app_mon_rs485 = on;
        else { app_reply("err mon can|rs485"); return; }
        app_reply("ok mon %s %s", v[1], v[2]);
    }
    else app_reply("err unknown cmd (try: help)");
}
