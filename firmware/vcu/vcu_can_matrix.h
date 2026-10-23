/**
 * @file vcu_can_matrix.h
 * @brief VCU CAN Bus Matrix (500 kbps) Network Manager
 */

#ifndef VCU_CAN_MATRIX_H
#define VCU_CAN_MATRIX_H

#include <stdint.h>
#include <stdbool.h>
#include "../../can_network/can_ids.h"
#include "../hal_abstraction/hal_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    CAN_Msg_BMS_PackStatus_t    bms_pack;
    CAN_Msg_BMS_LimitsAlarms_t  bms_limits;
    CAN_Msg_BMS_CellExtremes_t  bms_extremes;
    CAN_Msg_INV_Feedback_t      inv_feedback;
    CAN_Msg_INV_Thermal_t       inv_thermal;
    CAN_Msg_ABS_TCS_Status_t    abs_status;
    CAN_Msg_OBC_Status_t        obc_status;
    CAN_Msg_IMD_Status_t        imd_status;

    uint32_t last_rx_bms_ms;
    uint32_t last_rx_inv_ms;
    uint32_t last_rx_imd_ms;
    uint32_t last_rx_obc_ms;

    uint32_t last_tx_10ms;
    uint32_t last_tx_100ms;
    uint8_t  rolling_counter;
} VCU_CAN_Matrix_t;

extern VCU_CAN_Matrix_t g_vcu_can;

void VCU_CAN_Init(void);
void VCU_CAN_RX_Callback(const HAL_CAN_Message_t *msg);
void VCU_CAN_TX_Task_10ms(void);
void VCU_CAN_TX_Task_100ms(void);

#ifdef __cplusplus
}
#endif

#endif /* VCU_CAN_MATRIX_H */
