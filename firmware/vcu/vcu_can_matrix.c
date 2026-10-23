/**
 * @file vcu_can_matrix.c
 * @brief VCU CAN Matrix Implementation: Message Parsing and Transmission
 */

#include "vcu_can_matrix.h"
#include "vcu_main.h"
#include "vcu_safety_precharge.h"
#include "vcu_aux_control.h"
#include <string.h>

VCU_CAN_Matrix_t g_vcu_can;

void VCU_CAN_Init(void) {
    memset(&g_vcu_can, 0, sizeof(VCU_CAN_Matrix_t));
    HAL_CAN_SetRxCallback(VCU_CAN_RX_Callback);
}

/**
 * @brief CAN Receive Interrupt Callback (Zero-copy parsing into matrix structs)
 */
void VCU_CAN_RX_Callback(const HAL_CAN_Message_t *msg) {
    if (!msg) return;
    uint32_t now = HAL_Platform_GetTickMs();

    switch (msg->id) {
        case CAN_ID_BMS_PACK_STATUS:
            if (msg->dlc == sizeof(CAN_Msg_BMS_PackStatus_t)) {
                memcpy(&g_vcu_can.bms_pack, msg->data, msg->dlc);
                g_vcu_can.last_rx_bms_ms = now;

                /* Update VCU context voltages */
                g_vcu_ctx.battery_voltage_v = (float)g_vcu_can.bms_pack.pack_voltage_0_1V * 0.1f;
                g_vcu_ctx.bms_contactor_allow = (g_vcu_can.bms_pack.contactor_req_fault & 0x01) != 0;
            }
            break;

        case CAN_ID_BMS_LIMITS_ALARMS:
            if (msg->dlc == sizeof(CAN_Msg_BMS_LimitsAlarms_t)) {
                memcpy(&g_vcu_can.bms_limits, msg->data, msg->dlc);
                /* If critical alarm flag is set, trigger VCU safety fault */
                if (g_vcu_can.bms_limits.alarm_flags & (BMS_FAULT_OVER_VOLT | BMS_FAULT_UNDER_VOLT | BMS_FAULT_OVER_TEMP)) {
                    VCU_InjectFault(VCU_FAULT_BMS_EMERGENCY);
                }
            }
            break;

        case CAN_ID_BMS_CELL_EXTREMES:
            if (msg->dlc == sizeof(CAN_Msg_BMS_CellExtremes_t)) {
                memcpy(&g_vcu_can.bms_extremes, msg->data, msg->dlc);
            }
            break;

        case CAN_ID_INV_MOTOR_FEEDBACK:
            if (msg->dlc == sizeof(CAN_Msg_INV_Feedback_t)) {
                memcpy(&g_vcu_can.inv_feedback, msg->data, msg->dlc);
                g_vcu_can.last_rx_inv_ms = now;
                g_vcu_ctx.dc_bus_voltage_v = (float)g_vcu_can.inv_feedback.dc_bus_voltage_0_1V * 0.1f;
                g_vcu_ctx.inverter_ready = true;
            }
            break;

        case CAN_ID_INV_THERMAL_STATUS:
            if (msg->dlc == sizeof(CAN_Msg_INV_Thermal_t)) {
                memcpy(&g_vcu_can.inv_thermal, msg->data, msg->dlc);
            }
            break;

        case CAN_ID_ABS_TCS_STATUS:
            if (msg->dlc == sizeof(CAN_Msg_ABS_TCS_Status_t)) {
                memcpy(&g_vcu_can.abs_status, msg->data, msg->dlc);
            }
            break;

        case CAN_ID_OBC_STATUS_CONTROL:
            if (msg->dlc == sizeof(CAN_Msg_OBC_Status_t)) {
                memcpy(&g_vcu_can.obc_status, msg->data, msg->dlc);
                g_vcu_can.last_rx_obc_ms = now;
            }
            break;

        case CAN_ID_IMD_STATUS:
            if (msg->dlc == sizeof(CAN_Msg_IMD_Status_t)) {
                memcpy(&g_vcu_can.imd_status, msg->data, msg->dlc);
                g_vcu_can.last_rx_imd_ms = now;
                g_vcu_ctx.imd_isolation_ok = (g_vcu_can.imd_status.isolation_fault == 0);

                if (g_vcu_can.imd_status.isolation_fault != 0) {
                    VCU_InjectFault(VCU_FAULT_IMD_ISOLATION);
                }
            }
            break;

        default:
            break;
    }
}

/**
 * @brief High Priority 10ms Transmission Task (Heartbeat & Torque Command)
 */
void VCU_CAN_TX_Task_10ms(void) {
    /* 1. Heartbeat message */
    CAN_Msg_VCU_Heartbeat_t hb;
    hb.state = (uint8_t)g_vcu_ctx.current_state;
    hb.fault_code = g_vcu_ctx.fault_flags;
    hb.drive_mode = (uint8_t)g_vcu_ctx.drive_mode;
    hb.rolling_counter = g_vcu_can.rolling_counter++;
    hb.flags = 0;
    if (g_vcu_ctx.brake_pressed) hb.flags |= 0x01;
    if (g_vcu_ctx.current_state == VCU_STATE_READY_TO_DRIVE) hb.flags |= 0x02;
    hb.reserved = 0;

    HAL_CAN_Message_t tx_msg;
    tx_msg.id = CAN_ID_VCU_HEARTBEAT_STATE;
    tx_msg.is_extended = 0;
    tx_msg.dlc = sizeof(CAN_Msg_VCU_Heartbeat_t);
    memcpy(tx_msg.data, &hb, tx_msg.dlc);
    HAL_CAN_Transmit(&tx_msg);

    /* 2. Torque command message to Inverter */
    CAN_Msg_VCU_TorqueCommand_t cmd;
    cmd.target_torque_0_1Nm = (int16_t)(g_vcu_ctx.requested_torque_nm * 10.0f);
    cmd.control_flags = 0;
    if (g_vcu_ctx.current_state == VCU_STATE_READY_TO_DRIVE) {
        cmd.control_flags |= 0x01; /* Enable Inverter */
        cmd.control_flags |= 0x02; /* Forward */
        cmd.control_flags |= 0x04; /* Regen Allowed */
    }
    cmd.reserved1 = 0;
    cmd.max_speed_rpm = 6500;
    cmd.reserved2 = 0;

    tx_msg.id = CAN_ID_VCU_TORQUE_COMMAND;
    tx_msg.is_extended = 0;
    tx_msg.dlc = sizeof(CAN_Msg_VCU_TorqueCommand_t);
    memcpy(tx_msg.data, &cmd, tx_msg.dlc);
    HAL_CAN_Transmit(&tx_msg);
}

/**
 * @brief 100ms Periodic Aux Status Transmission
 */
void VCU_CAN_TX_Task_100ms(void) {
    CAN_Msg_VCU_AuxStatus_t aux;
    aux.cooling_pump_pwm = (uint8_t)g_aux_ctrl.cooling_pump_pwm_pct;
    aux.radiator_fan_pwm = (uint8_t)g_aux_ctrl.radiator_fan_pwm_pct;
    aux.contactors_lights = 0;
    if (g_aux_ctrl.headlights_active) aux.contactors_lights |= (1 << 0);
    if (g_aux_ctrl.brakelights_active) aux.contactors_lights |= (1 << 1);
    if (g_precharge_ctrl.precharge_relay_closed) aux.contactors_lights |= (1 << 2);
    if (g_precharge_ctrl.main_pos_closed) aux.contactors_lights |= (1 << 3);
    if (g_precharge_ctrl.main_neg_closed) aux.contactors_lights |= (1 << 4);
    memset(aux.reserved, 0, sizeof(aux.reserved));

    HAL_CAN_Message_t tx_msg;
    tx_msg.id = CAN_ID_VCU_AUX_STATUS;
    tx_msg.is_extended = 0;
    tx_msg.dlc = sizeof(CAN_Msg_VCU_AuxStatus_t);
    memcpy(tx_msg.data, &aux, tx_msg.dlc);
    HAL_CAN_Transmit(&tx_msg);
}
