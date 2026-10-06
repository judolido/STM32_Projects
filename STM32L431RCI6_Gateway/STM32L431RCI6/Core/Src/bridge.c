/* CAN (bxCAN, CAN1) + RS485 (USART3) capture and transmit. */
#include "bridge.h"
#include "main.h"
#include <string.h>

extern CAN_HandleTypeDef  hcan1;
extern UART_HandleTypeDef huart3;

static bridge_stats_t st;
static can_frame_t canq[CAN_RING_SIZE];
static volatile uint16_t can_h, can_t;
static rs485_frame_t rsq[RS485_RING];
static volatile uint8_t rs_h, rs_t;
static uint8_t rs_buf[RS485_RX_BUF];
static uint32_t cur_kbps, cur_mode;

/* ------------------------------ CAN ------------------------------ */
int bridge_can_set(uint32_t mode, uint32_t kbps) {
    if (!kbps) return -1;
    uint32_t div = kbps * 16UL;                       /* 16 time quanta per bit */
    if (CAN_CLK_KHZ % div) return -1;
    uint32_t presc = CAN_CLK_KHZ / div;
    if (presc < 1 || presc > 1024) return -1;

    HAL_CAN_Stop(&hcan1);                             /* ignore error if not started */
    hcan1.Init.Prescaler = presc;
    hcan1.Init.Mode = mode;
    hcan1.Init.SyncJumpWidth = CAN_SJW_1TQ;
    hcan1.Init.TimeSeg1 = CAN_BS1_13TQ;               /* sample point 87.5 % */
    hcan1.Init.TimeSeg2 = CAN_BS2_2TQ;
    hcan1.Init.AutoBusOff = ENABLE;
    hcan1.Init.AutoRetransmission = ENABLE;
    if (HAL_CAN_Init(&hcan1) != HAL_OK) return -2;

    CAN_FilterTypeDef f = {0};
    f.FilterBank = 0; f.FilterMode = CAN_FILTERMODE_IDMASK; f.FilterScale = CAN_FILTERSCALE_32BIT;
    f.FilterFIFOAssignment = CAN_RX_FIFO0; f.FilterActivation = ENABLE; f.SlaveStartFilterBank = 14;
    if (HAL_CAN_ConfigFilter(&hcan1, &f) != HAL_OK) return -3;       /* accept everything */
    if (HAL_CAN_Start(&hcan1) != HAL_OK) return -4;
    HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING | CAN_IT_RX_FIFO0_OVERRUN | CAN_IT_BUSOFF);
    cur_kbps = kbps; cur_mode = mode;
    return 0;
}
uint32_t bridge_can_kbps(void) { return cur_kbps; }
uint32_t bridge_can_mode(void) { return cur_mode; }
const char *bridge_can_mode_str(void) {
    switch (cur_mode) {
    case CAN_MODE_NORMAL: return "normal";
    case CAN_MODE_SILENT: return "silent";
    case CAN_MODE_LOOPBACK: return "loopback";
    default: return "silentloop";
    }
}

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *h) {
    CAN_RxHeaderTypeDef rh; uint8_t d[8];
    while (HAL_CAN_GetRxFifoFillLevel(h, CAN_RX_FIFO0)) {
        if (HAL_CAN_GetRxMessage(h, CAN_RX_FIFO0, &rh, d) != HAL_OK) break;
        uint16_t nh = (can_h + 1) & (CAN_RING_SIZE - 1);
        if (nh == can_t) { st.can_drop++; continue; }
        can_frame_t *f = &canq[can_h];
        f->ts = HAL_GetTick();
        f->ext = (rh.IDE == CAN_ID_EXT);
        f->id = f->ext ? rh.ExtId : rh.StdId;
        f->rtr = (rh.RTR == CAN_RTR_REMOTE);
        f->dlc = (uint8_t)rh.DLC;
        memcpy(f->data, d, 8);
        can_h = nh; st.can_rx++;
    }
}
void HAL_CAN_ErrorCallback(CAN_HandleTypeDef *h) { (void)h; st.can_err++; }

bool bridge_can_pop(can_frame_t *f) {
    if (can_t == can_h) return false;
    *f = canq[can_t]; can_t = (can_t + 1) & (CAN_RING_SIZE - 1); return true;
}
uint16_t bridge_can_pending(void) { return (can_h - can_t) & (CAN_RING_SIZE - 1); }

int bridge_can_send(uint32_t id, bool ext, bool rtr, const uint8_t *d, uint8_t dlc) {
    CAN_TxHeaderTypeDef h = {0}; uint32_t mb; uint8_t buf[8] = {0};
    if (dlc > 8) return -1;
    if (ext) { h.IDE = CAN_ID_EXT; h.ExtId = id; } else { h.IDE = CAN_ID_STD; h.StdId = id; }
    h.RTR = rtr ? CAN_RTR_REMOTE : CAN_RTR_DATA; h.DLC = dlc; h.TransmitGlobalTime = DISABLE;
    if (d && dlc) memcpy(buf, d, dlc);
    if (HAL_CAN_GetTxMailboxesFreeLevel(&hcan1) == 0) return -2;
    if (HAL_CAN_AddTxMessage(&hcan1, &h, buf, &mb) != HAL_OK) return -3;
    st.can_tx++; return 0;
}

/* ------------------------------ RS485 ------------------------------ */
static void rs_start_rx(void) { HAL_UARTEx_ReceiveToIdle_IT(&huart3, rs_buf, RS485_RX_BUF); }

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *hu, uint16_t size) {
    if (hu != &huart3) return;
    if (size) {
        uint8_t nh = (rs_h + 1) % RS485_RING;
        if (nh == rs_t) st.rs_drop++;
        else {
            rs485_frame_t *f = &rsq[rs_h];
            f->ts = HAL_GetTick(); f->len = size; memcpy(f->data, rs_buf, size);
            rs_h = nh; st.rs_frames++; st.rs_bytes += size;
        }
    }
    rs_start_rx();
}
void HAL_UART_ErrorCallback(UART_HandleTypeDef *hu) {
    if (hu != &huart3) return;
    st.rs_err++;
    HAL_UART_AbortReceive(&huart3);
    rs_start_rx();
}
bool bridge_rs485_pop(rs485_frame_t *f) {
    if (rs_t == rs_h) return false;
    *f = rsq[rs_t]; rs_t = (rs_t + 1) % RS485_RING; return true;
}
int bridge_rs485_send(const uint8_t *d, uint16_t n) {
#ifdef RS485_DE_MODE_GPIO
    HAL_GPIO_WritePin(RS485_DE_PORT, RS485_DE_PIN, GPIO_PIN_SET);
#endif
    HAL_StatusTypeDef r = HAL_UART_Transmit(&huart3, (uint8_t *)d, n, 1000);  /* returns after TC */
#ifdef RS485_DE_MODE_GPIO
    HAL_GPIO_WritePin(RS485_DE_PORT, RS485_DE_PIN, GPIO_PIN_RESET);
#endif
    return r == HAL_OK ? 0 : -1;
}
int bridge_rs485_set_baud(uint32_t baud) {
    if (baud < 1200 || baud > 2000000) return -1;
    HAL_UART_AbortReceive(&huart3);
    huart3.Init.BaudRate = baud;
    huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;      /* RTS flow control must be OFF for DE mode */
#ifdef RS485_DE_MODE_HW
    /* PD2 = USART3_RTS pin used as ADM483 DE (active high), 16/16 sample-time guard */
    HAL_StatusTypeDef r = HAL_RS485Ex_Init(&huart3, UART_DE_POLARITY_HIGH, 16, 16);
#else
    HAL_StatusTypeDef r = HAL_UART_Init(&huart3);
#endif
    rs_start_rx();
    return r == HAL_OK ? 0 : -2;
}

/* The project has no CAN1/USART3 handlers in stm32l4xx_it.c, so they live here.
   (If you later tick these interrupts in CubeMX, delete these two functions.) */
void CAN1_RX0_IRQHandler(void) { HAL_CAN_IRQHandler(&hcan1); }
void USART3_IRQHandler(void)   { HAL_UART_IRQHandler(&huart3); }

void bridge_init(void) {
    /* TJA1050 pin S via CAN1_CTR: low = high-speed (normal) transceiver mode */
    HAL_GPIO_WritePin(CAN1_CTR_GPIO_Port, CAN1_CTR_Pin, GPIO_PIN_RESET);
    HAL_NVIC_SetPriority(CAN1_RX0_IRQn, 5, 0); HAL_NVIC_EnableIRQ(CAN1_RX0_IRQn);
    HAL_NVIC_SetPriority(USART3_IRQn, 5, 0);   HAL_NVIC_EnableIRQ(USART3_IRQn);
    bridge_can_set(CAN_DEFAULT_MODE, CAN_DEFAULT_KBPS);
    bridge_rs485_set_baud(RS485_DEFAULT_BAUD);
}
const bridge_stats_t *bridge_stats(void) { return &st; }
