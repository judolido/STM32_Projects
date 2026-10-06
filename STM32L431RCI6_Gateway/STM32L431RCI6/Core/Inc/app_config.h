#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/* ---- Per-board identity: change DEVICE_NUM for every board (1, 2, ...) ---- */
#define DEVICE_NUM        1
#define APP_STR_(x)       #x
#define APP_STR(x)        APP_STR_(x)
#define DEVICE_NAME       "stm32-" APP_STR(DEVICE_NUM)
/* printf debug on USART1 (115200 8N1). Set 0 to silence. */
#define APP_DEBUG         1
#define FW_VERSION        "0.1.0"
#define MAC_ADDR          {0x02, 0x08, 0xDC, 0x11, 0x22, DEVICE_NUM}

/* ---- Broker (laptop). Give the laptop a static IP / DHCP reservation ---- */
/* ---- Addressing. Static avoids the two-DHCP-servers problem on this LAN ---- */
#define USE_DHCP          0
#define NET_IP            {192, 168, 88, (2 + DEVICE_NUM)}   /* board 1 = .88.3, board 2 = .88.4 */
#define NET_SN            {255, 255, 255, 0}
#define NET_GW            {192, 168, 88, 1}                  /* MikroTik */
#define BROKER_IP_0       192
#define BROKER_IP_1       168
#define BROKER_IP_2       88
#define BROKER_IP_3       9                                  /* laptop's 2nd (secondary) address */
#define BROKER_IP         {BROKER_IP_0, BROKER_IP_1, BROKER_IP_2, BROKER_IP_3}
#define BROKER_PORT       1883
#define MQTT_KEEPALIVE_S  30
#define MQTT_RETRY_MS     3000

/* ---- Topics ---- */
#define TOPIC_BASE        "site/" DEVICE_NAME "/"
#define TOPIC_ALL_CMD     "site/all/cmd"

/* ---- W5500 sockets ---- */
#define SOCK_DHCP         0
#define SOCK_MQTT         1
#define SOCK_UDP          2
#define UDP_PORT          5000      /* chip-to-chip / laptop UDP messages */

/* ---- CAN ---- */
#define CAN_CLK_KHZ       80000UL   /* APB1 clock in kHz */
#define CAN_DEFAULT_KBPS  500
#define CAN_DEFAULT_MODE  CAN_MODE_SILENT   /* silent = pure sniffer, never ACKs */
#define CAN_RING_SIZE     128       /* power of 2 */
#define CAN_BATCH_MS      50
#define CAN_BATCH_MAX     10

/* ---- RS485 (USART3) ---- */
#define RS485_RX_BUF      128
#define RS485_RING        16
#define RS485_DEFAULT_BAUD 115200
/* Direction control. Your .ioc: PD2 = USART3_RTS -> ADM483 DE, so use HW driver-enable mode.
   (Code forces HwFlowCtl=NONE + HAL_RS485Ex_Init; plain RTS flow control is not RS485 DE.) */
#define RS485_DE_MODE_HW
/* #define RS485_DE_MODE_GPIO
   #define RS485_DE_PORT GPIOB
   #define RS485_DE_PIN  GPIO_PIN_14 */

#endif
