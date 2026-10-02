#include "w5500.h"
#include "w5500_regs.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

static SPI_HandleTypeDef  *s_hspi     = NULL;
static UART_HandleTypeDef *s_huartdbg = NULL;

/* ------------------------------------------------------------------ */
/*  Debug print helper — writes directly to the UART handle, no printf/ */
/*  syscalls retargeting involved, so it can never silently break.      */
/* ------------------------------------------------------------------ */
static void dbg_print(const char *fmt, ...)
{
    if (s_huartdbg == NULL)
        return;

    char buf[96];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (len > 0)
    {
        HAL_UART_Transmit(s_huartdbg, (uint8_t *)buf,
                           (len < (int)sizeof(buf)) ? (uint16_t)len : (uint16_t)(sizeof(buf) - 1),
                           HAL_MAX_DELAY);
    }
}

/* ------------------------------------------------------------------ */
/*  Low-level SPI transaction primitives                                */
/* ------------------------------------------------------------------ */
static inline void cs_low(void)  { HAL_GPIO_WritePin(W5500_CS_PORT, W5500_CS_PIN, GPIO_PIN_RESET); }
static inline void cs_high(void) { HAL_GPIO_WritePin(W5500_CS_PORT, W5500_CS_PIN, GPIO_PIN_SET);   }

static void w5500_write(uint16_t addr, uint8_t ctrl, const uint8_t *data, uint16_t len)
{
    uint8_t header[3];
    header[0] = (uint8_t)(addr >> 8);
    header[1] = (uint8_t)(addr & 0xFF);
    header[2] = ctrl;

    cs_low();
    HAL_SPI_Transmit(s_hspi, header, 3, HAL_MAX_DELAY);
    if (len > 0)
        HAL_SPI_Transmit(s_hspi, (uint8_t *)data, len, HAL_MAX_DELAY);
    cs_high();
}

static void w5500_read(uint16_t addr, uint8_t ctrl, uint8_t *data, uint16_t len)
{
    uint8_t header[3];
    header[0] = (uint8_t)(addr >> 8);
    header[1] = (uint8_t)(addr & 0xFF);
    header[2] = ctrl;

    cs_low();
    HAL_SPI_Transmit(s_hspi, header, 3, HAL_MAX_DELAY);
    if (len > 0)
        HAL_SPI_Receive(s_hspi, data, len, HAL_MAX_DELAY);
    cs_high();
}

static inline void w5500_write_reg8(uint16_t addr, uint8_t val)
{
    w5500_write(addr, W5500_CTRL_COMMON_WRITE, &val, 1);
}

static inline uint8_t w5500_read_reg8(uint16_t addr)
{
    uint8_t val = 0;
    w5500_read(addr, W5500_CTRL_COMMON_READ, &val, 1);
    return val;
}

/* ------------------------------------------------------------------ */
/*  Reset                                                                */
/* ------------------------------------------------------------------ */
void W5500_Reset(void)
{
    cs_high(); /* idle-high: chip deselected before/during reset */
    HAL_GPIO_WritePin(W5500_RST_PORT, W5500_RST_PIN, GPIO_PIN_RESET);
    HAL_Delay(1);   /* datasheet requires >= 500 us low pulse */
    HAL_GPIO_WritePin(W5500_RST_PORT, W5500_RST_PIN, GPIO_PIN_SET);
    HAL_Delay(2);   /* PLL lock time before first SPI access  */
}

/* ------------------------------------------------------------------ */
/*  Init                                                                 */
/* ------------------------------------------------------------------ */
uint8_t W5500_Init(SPI_HandleTypeDef *hspi, UART_HandleTypeDef *huart_dbg)
{
    s_hspi     = hspi;
    s_huartdbg = huart_dbg;

    /* CS and RESET must already be configured as GPIO_OUTPUT_PP in
     * MX_GPIO_Init() (idle level for CS = high, RESET = high). */
    dbg_print("\r\n[W5500] Resetting chip...\r\n");
    W5500_Reset();

    uint8_t version = w5500_read_reg8(W5500_REG_VERSIONR);
    dbg_print("[W5500] VERSIONR = 0x%02X (expect 0x04)\r\n", version);

    if (version != 0x04)
    {
        dbg_print("[W5500] ERROR: chip not responding over SPI. Check wiring, "
                   "CS/RESET pin config, and SPI DataSize (must be 8-bit).\r\n");
        return 0;
    }

    dbg_print("[W5500] SPI communication OK.\r\n");
    return 1;
}

/* ------------------------------------------------------------------ */
/*  Network configuration                                               */
/* ------------------------------------------------------------------ */
void W5500_SetMAC(const uint8_t mac[6])
{
    w5500_write(W5500_REG_SHAR, W5500_CTRL_COMMON_WRITE, mac, 6);
}

void W5500_GetMAC(uint8_t mac[6])
{
    w5500_read(W5500_REG_SHAR, W5500_CTRL_COMMON_READ, mac, 6);
}

void W5500_SetIP(const uint8_t ip[4], const uint8_t subnet[4], const uint8_t gateway[4])
{
    w5500_write(W5500_REG_SIPR, W5500_CTRL_COMMON_WRITE, ip, 4);
    w5500_write(W5500_REG_SUBR, W5500_CTRL_COMMON_WRITE, subnet, 4);
    w5500_write(W5500_REG_GAR,  W5500_CTRL_COMMON_WRITE, gateway, 4);
}

void W5500_GetIP(uint8_t ip[4])
{
    w5500_read(W5500_REG_SIPR, W5500_CTRL_COMMON_READ, ip, 4);
}

/* ------------------------------------------------------------------ */
/*  Link / status                                                       */
/* ------------------------------------------------------------------ */
uint8_t W5500_IsLinkUp(void)
{
    uint8_t phycfgr = w5500_read_reg8(W5500_REG_PHYCFGR);
    return (phycfgr & W5500_PHYCFGR_LNK) ? 1 : 0;
}

void W5500_GetStatus(W5500_Status_t *status)
{
    memset(status, 0, sizeof(*status));

    W5500_GetMAC(status->mac);
    W5500_GetIP(status->ip);
    w5500_read(W5500_REG_GAR,  W5500_CTRL_COMMON_READ, status->gw, 4);
    w5500_read(W5500_REG_SUBR, W5500_CTRL_COMMON_READ, status->sn, 4);

    uint8_t phycfgr = w5500_read_reg8(W5500_REG_PHYCFGR);
    status->link_up     = (phycfgr & W5500_PHYCFGR_LNK) ? 1 : 0;
    status->speed_100m  = (phycfgr & W5500_PHYCFGR_SPD) ? 1 : 0;
    status->full_duplex = (phycfgr & W5500_PHYCFGR_DPX) ? 1 : 0;

    status->chip_version = w5500_read_reg8(W5500_REG_VERSIONR);
}

void W5500_PrintStatus(void)
{
    W5500_Status_t st;
    W5500_GetStatus(&st);

    dbg_print("---- W5500 Status ----\r\n");
    dbg_print("Chip version : 0x%02X\r\n", st.chip_version);
    dbg_print("MAC          : %02X:%02X:%02X:%02X:%02X:%02X\r\n",
               st.mac[0], st.mac[1], st.mac[2], st.mac[3], st.mac[4], st.mac[5]);
    dbg_print("IP           : %d.%d.%d.%d\r\n", st.ip[0], st.ip[1], st.ip[2], st.ip[3]);
    dbg_print("Subnet       : %d.%d.%d.%d\r\n", st.sn[0], st.sn[1], st.sn[2], st.sn[3]);
    dbg_print("Gateway      : %d.%d.%d.%d\r\n", st.gw[0], st.gw[1], st.gw[2], st.gw[3]);
    dbg_print("Link         : %s\r\n", st.link_up ? "UP" : "DOWN");
    dbg_print("Speed        : %s\r\n", st.speed_100m ? "100 Mbps" : "10 Mbps");
    dbg_print("Duplex       : %s\r\n", st.full_duplex ? "Full" : "Half");
    dbg_print("-----------------------\r\n");
}
