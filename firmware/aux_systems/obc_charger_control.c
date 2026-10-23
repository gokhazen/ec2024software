/**
 * @file obc_charger_control.c
 * @brief On-Board Charger (OBC) Implementation: J1772 Pilot Decoding & CC/CV Profile
 */

#include "obc_charger_control.h"
#include "../hal_abstraction/hal_platform.h"

OBC_Controller_t g_obc_ctrl;

#define CC_CUTOFF_CELL_MV 4180.0f /* Transition to CV when cell reaches 4.18V */
#define CHARGE_COMPLETE_A 1.5f    /* Stop charging when current drops below 1.5A in CV */

void OBC_Init(float pack_cv_target_voltage_v) {
    g_obc_ctrl.state = OBC_STATE_IDLE;
    g_obc_ctrl.plug_detected = false;
    g_obc_ctrl.drive_lockout_active = false;
    g_obc_ctrl.pilot_duty_cycle_pct = 0.0f;
    g_obc_ctrl.grid_max_current_a = 0.0f;
    g_obc_ctrl.output_voltage_v = 0.0f;
    g_obc_ctrl.output_current_a = 0.0f;
    g_obc_ctrl.target_current_a = 0.0f;
    g_obc_ctrl.target_voltage_v = pack_cv_target_voltage_v;
    g_obc_ctrl.charge_start_time_ms = 0;
    g_obc_ctrl.energy_delivered_wh = 0;
}

/**
 * @brief Decodes SAE J1772 / IEC 62196 Control Pilot PWM
 */
void OBC_Process_Pilot_PWM(float duty_cycle_pct, bool proximity_plug_present) {
    g_obc_ctrl.plug_detected = proximity_plug_present;
    g_obc_ctrl.pilot_duty_cycle_pct = duty_cycle_pct;

    /* If plug is physically inserted, drive lockout is immediately locked */
    g_obc_ctrl.drive_lockout_active = proximity_plug_present;

    if (!proximity_plug_present) {
        g_obc_ctrl.grid_max_current_a = 0.0f;
        g_obc_ctrl.state = OBC_STATE_IDLE;
        return;
    }

    /* Standard SAE J1772 Duty Cycle to Amperes formula */
    if (duty_cycle_pct >= 10.0f && duty_cycle_pct <= 85.0f) {
        g_obc_ctrl.grid_max_current_a = duty_cycle_pct * 0.6f;
    } else if (duty_cycle_pct > 85.0f && duty_cycle_pct <= 96.0f) {
        g_obc_ctrl.grid_max_current_a = (duty_cycle_pct - 64.0f) * 2.5f;
    } else {
        g_obc_ctrl.grid_max_current_a = 0.0f; /* Invalid duty cycle */
    }
}

/**
 * @brief 50ms Periodic Charging Control Loop (CC / CV Execution)
 */
void OBC_Task_50ms_Loop(float pack_voltage_v, float bms_allowed_charge_current_a, float max_cell_mv) {
    if (!g_obc_ctrl.plug_detected || g_obc_ctrl.grid_max_current_a <= 1.0f) {
        g_obc_ctrl.output_current_a = 0.0f;
        g_obc_ctrl.state = OBC_STATE_IDLE;
        return;
    }

    /* Maximum allowed current is the minimum of Grid Limit and BMS Battery Limit */
    float safe_max_current = g_obc_ctrl.grid_max_current_a;
    if (bms_allowed_charge_current_a < safe_max_current) {
        safe_max_current = bms_allowed_charge_current_a;
    }

    switch (g_obc_ctrl.state) {
        case OBC_STATE_IDLE:
            g_obc_ctrl.state = OBC_STATE_CONNECTED_WAIT_PILOT;
            break;

        case OBC_STATE_CONNECTED_WAIT_PILOT:
            if (safe_max_current > 2.0f) {
                g_obc_ctrl.state = OBC_STATE_CONSTANT_CURRENT;
                g_obc_ctrl.charge_start_time_ms = HAL_Platform_GetTickMs();
            }
            break;

        case OBC_STATE_CONSTANT_CURRENT:
            /* CC Mode: Hold target current at maximum safe limit */
            g_obc_ctrl.target_current_a = safe_max_current;
            g_obc_ctrl.output_current_a = safe_max_current;
            g_obc_ctrl.output_voltage_v = pack_voltage_v;

            /* When highest cell approaches 4.18V, switch to Constant Voltage */
            if (max_cell_mv >= CC_CUTOFF_CELL_MV || pack_voltage_v >= g_obc_ctrl.target_voltage_v) {
                g_obc_ctrl.state = OBC_STATE_CONSTANT_VOLTAGE;
            }
            break;

        case OBC_STATE_CONSTANT_VOLTAGE:
            /* CV Mode: Regulate pack voltage to target, current tapers down */
            g_obc_ctrl.output_voltage_v = g_obc_ctrl.target_voltage_v;
            
            /* Simulated natural battery taper current */
            if (g_obc_ctrl.output_current_a > 1.0f) {
                g_obc_ctrl.output_current_a -= 0.15f;
            }

            /* Cutoff condition */
            if (g_obc_ctrl.output_current_a <= CHARGE_COMPLETE_A) {
                g_obc_ctrl.output_current_a = 0.0f;
                g_obc_ctrl.state = OBC_STATE_CHARGE_COMPLETE;
            }
            break;

        case OBC_STATE_CHARGE_COMPLETE:
            g_obc_ctrl.output_current_a = 0.0f;
            break;

        case OBC_STATE_FAULT:
        default:
            g_obc_ctrl.output_current_a = 0.0f;
            break;
    }
}

const char* OBC_StateToString(OBC_State_t state) {
    switch (state) {
        case OBC_STATE_IDLE:                   return "IDLE";
        case OBC_STATE_CONNECTED_WAIT_PILOT:   return "WAIT_PILOT";
        case OBC_STATE_PRECHARGE:              return "PRECHARGE";
        case OBC_STATE_CONSTANT_CURRENT:       return "CONSTANT_CURRENT";
        case OBC_STATE_CONSTANT_VOLTAGE:       return "CONSTANT_VOLTAGE";
        case OBC_STATE_CHARGE_COMPLETE:        return "CHARGE_COMPLETE";
        case OBC_STATE_FAULT:                  return "FAULT";
        default:                               return "UNKNOWN";
    }
}
