/**
 * @file vcu_main.c
 * @brief VCU Core State Machine and Master Executive Loop
 */

#include "vcu_main.h"
#include "vcu_safety_precharge.h"
#include "vcu_pedal_map.h"
#include "vcu_can_matrix.h"
#include "vcu_aux_control.h"
#include "../hal_abstraction/hal_platform.h"
#include <stdio.h>

VCU_Context_t g_vcu_ctx;

void VCU_Init(void) {
    g_vcu_ctx.current_state = VCU_STATE_INIT;
    g_vcu_ctx.previous_state = VCU_STATE_INIT;
    g_vcu_ctx.drive_mode = DRIVE_MODE_NORMAL;
    g_vcu_ctx.fault_flags = VCU_FAULT_NONE;
    
    g_vcu_ctx.ignition_switch = false;
    g_vcu_ctx.emergency_stop_pressed = false;
    g_vcu_ctx.bms_contactor_allow = false;
    g_vcu_ctx.imd_isolation_ok = true;
    g_vcu_ctx.inverter_ready = false;
    g_vcu_ctx.brake_pressed = false;
    
    g_vcu_ctx.battery_voltage_v = 0.0f;
    g_vcu_ctx.dc_bus_voltage_v = 0.0f;
    
    g_vcu_ctx.throttle_pct = 0.0f;
    g_vcu_ctx.brake_pct = 0.0f;
    g_vcu_ctx.requested_torque_nm = 0.0f;
    
    g_vcu_ctx.state_timer_ms = 0;
    g_vcu_ctx.last_tick_ms = HAL_Platform_GetTickMs();

    /* Initialize Subsystems */
    Precharge_Init();
    Pedal_Init();
    Aux_Control_Init();
    VCU_CAN_Init();

    /* Transition to STANDBY after successful peripheral initialization */
    g_vcu_ctx.current_state = VCU_STATE_STANDBY;
}

/**
 * @brief Main 10ms Periodic State Machine Task
 */
void VCU_Task_10ms_State_Machine(void) {
    uint32_t now = HAL_Platform_GetTickMs();
    uint32_t dt = now - g_vcu_ctx.last_tick_ms;
    g_vcu_ctx.last_tick_ms = now;
    g_vcu_ctx.state_timer_ms += dt;

    /* Safety Interlock 1: Physical Emergency Stop Button */
    if (HAL_GPIO_ReadPin(HAL_PIN_EMERGENCY_STOP_SENSE) == HAL_PIN_HIGH) {
        g_vcu_ctx.emergency_stop_pressed = true;
        VCU_InjectFault(VCU_FAULT_EMERGENCY_STOP);
    }

    /* Safety Interlock 2: Hardware IMD Pin */
    if (HAL_GPIO_ReadPin(HAL_PIN_IMD_FAULT_SENSE) == HAL_PIN_HIGH) {
        g_vcu_ctx.imd_isolation_ok = false;
        VCU_InjectFault(VCU_FAULT_IMD_ISOLATION);
    }

    /* State Machine Execution */
    switch (g_vcu_ctx.current_state) {
        case VCU_STATE_INIT:
            /* Transient state during startup */
            g_vcu_ctx.current_state = VCU_STATE_STANDBY;
            break;

        case VCU_STATE_STANDBY:
            g_vcu_ctx.requested_torque_nm = 0.0f;
            /* When ignition key is turned ON and safety pre-conditions are met */
            if (g_vcu_ctx.ignition_switch && !g_vcu_ctx.emergency_stop_pressed && 
                g_vcu_ctx.fault_flags == VCU_FAULT_NONE) {
                
                if (g_vcu_ctx.battery_voltage_v > 50.0f) {
                    /* Begin precharge sequence */
                    Precharge_Start(g_vcu_ctx.battery_voltage_v);
                    g_vcu_ctx.previous_state = VCU_STATE_STANDBY;
                    g_vcu_ctx.current_state = VCU_STATE_PRECHARGE;
                    g_vcu_ctx.state_timer_ms = 0;
                }
            }
            break;

        case VCU_STATE_PRECHARGE:
            g_vcu_ctx.requested_torque_nm = 0.0f;
            
            /* If ignition turned off during precharge */
            if (!g_vcu_ctx.ignition_switch) {
                Contactor_Normal_Shutdown();
                g_vcu_ctx.current_state = VCU_STATE_STANDBY;
                break;
            }

            /* Advance precharge state */
            bool precharge_done = Precharge_Process_Run(g_vcu_ctx.dc_bus_voltage_v, now);
            if (precharge_done) {
                g_vcu_ctx.previous_state = VCU_STATE_PRECHARGE;
                g_vcu_ctx.current_state = VCU_STATE_READY_TO_DRIVE;
                g_vcu_ctx.state_timer_ms = 0;
            } else if (g_precharge_ctrl.substate == PRECHARGE_FAILED) {
                VCU_InjectFault(VCU_FAULT_PRECHARGE_TIMEOUT);
            }
            break;

        case VCU_STATE_READY_TO_DRIVE:
            /* If ignition turned off, perform clean shutdown */
            if (!g_vcu_ctx.ignition_switch) {
                g_vcu_ctx.requested_torque_nm = 0.0f;
                Contactor_Normal_Shutdown();
                g_vcu_ctx.current_state = VCU_STATE_STANDBY;
                break;
            }

            /* Read dual APPS pedal channels */
            float ch1 = HAL_ADC_ReadNormalized(0);
            float ch2 = HAL_ADC_ReadNormalized(1);
            float throttle_pct = 0.0f;
            bool pedal_ok = Read_Throttle_ADC(ch1, ch2, &throttle_pct);
            
            if (!pedal_ok) {
                VCU_InjectFault(VCU_FAULT_APPS_PLAUSIBILITY);
                g_vcu_ctx.requested_torque_nm = 0.0f;
                break;
            }
            g_vcu_ctx.throttle_pct = throttle_pct;

            /* Read brake pedal ADC */
            float brake_norm = HAL_ADC_ReadNormalized(2);
            g_vcu_ctx.brake_pct = brake_norm * 100.0f;
            g_vcu_ctx.brake_pressed = (g_vcu_ctx.brake_pct > 5.0f);

            /* Check Brake-Throttle Plausibility (Brake Override) */
            Brake_Plausibility_Check(g_vcu_ctx.throttle_pct, g_vcu_ctx.brake_pct);

            /* Dynamic derating limit from BMS / Inverter */
            float max_allowed_torque = MAX_MOTOR_TORQUE_NM;
            if (g_vcu_can.bms_limits.max_discharge_0_1A < 1000) {
                /* Derate proportional to discharge current limit */
                max_allowed_torque *= ((float)g_vcu_can.bms_limits.max_discharge_0_1A / 1000.0f);
            }

            /* Calculate Target Torque */
            g_vcu_ctx.requested_torque_nm = Calculate_Torque_Request(
                g_vcu_ctx.throttle_pct,
                g_vcu_ctx.brake_pct,
                g_vcu_ctx.drive_mode,
                max_allowed_torque
            );
            break;

        case VCU_STATE_FAULT:
        default:
            g_vcu_ctx.requested_torque_nm = 0.0f;
            Contactor_Emergency_Open("VCU In FAULT State");
            break;
    }

    /* Aux & Thermal Management */
    Aux_Thermal_Management_Task(
        (float)g_vcu_can.inv_thermal.motor_temp_c,
        (float)g_vcu_can.inv_thermal.inverter_temp_c,
        (float)g_vcu_can.bms_extremes.temp_max_c
    );
    Aux_SetLighting(g_aux_ctrl.headlights_active, g_vcu_ctx.brake_pressed);

    /* Transmit CAN Messages */
    VCU_CAN_TX_Task_10ms();
    static uint32_t last_100ms_tx = 0;
    if (now - last_100ms_tx >= 100) {
        last_100ms_tx = now;
        VCU_CAN_TX_Task_100ms();
    }
}

void VCU_SetDriveMode(Drive_Mode_t mode) {
    g_vcu_ctx.drive_mode = mode;
}

void VCU_InjectFault(VCU_FaultCode_t fault) {
    g_vcu_ctx.fault_flags |= (uint16_t)fault;
    g_vcu_ctx.previous_state = g_vcu_ctx.current_state;
    g_vcu_ctx.current_state = VCU_STATE_FAULT;
    g_vcu_ctx.requested_torque_nm = 0.0f;
    Contactor_Emergency_Open("Fault Injected");
}

void VCU_ClearFaults(void) {
    g_vcu_ctx.fault_flags = VCU_FAULT_NONE;
    if (g_vcu_ctx.current_state == VCU_STATE_FAULT) {
        g_vcu_ctx.current_state = VCU_STATE_STANDBY;
    }
}

const char* VCU_StateToString(VCU_State_t state) {
    switch (state) {
        case VCU_STATE_INIT:           return "INIT";
        case VCU_STATE_STANDBY:        return "STANDBY";
        case VCU_STATE_PRECHARGE:      return "PRECHARGE";
        case VCU_STATE_READY_TO_DRIVE: return "READY_TO_DRIVE";
        case VCU_STATE_FAULT:          return "FAULT";
        default:                       return "UNKNOWN";
    }
}
