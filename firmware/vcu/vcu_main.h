/**
 * @file vcu_main.h
 * @brief Vehicle Control Unit (VCU) Main Executive and State Machine Interface
 */

#ifndef VCU_MAIN_H
#define VCU_MAIN_H

#include <stdint.h>
#include <stdbool.h>
#include "../common/ev_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    VCU_State_t     current_state;
    VCU_State_t     previous_state;
    Drive_Mode_t    drive_mode;
    uint16_t        fault_flags;
    
    /* Sensor and Subsystem Status Inputs */
    bool            ignition_switch;
    bool            emergency_stop_pressed;
    bool            bms_contactor_allow;
    bool            imd_isolation_ok;
    bool            inverter_ready;
    bool            brake_pressed;
    
    /* Voltages for Precharge Evaluation */
    float           battery_voltage_v;
    float           dc_bus_voltage_v;
    
    /* Pedals & Outputs */
    float           throttle_pct;
    float           brake_pct;
    float           requested_torque_nm;
    
    uint32_t        state_timer_ms;
    uint32_t        last_tick_ms;
} VCU_Context_t;

extern VCU_Context_t g_vcu_ctx;

void VCU_Init(void);
void VCU_Task_10ms_State_Machine(void);
void VCU_SetDriveMode(Drive_Mode_t mode);
void VCU_InjectFault(VCU_FaultCode_t fault);
void VCU_ClearFaults(void);
const char* VCU_StateToString(VCU_State_t state);

#ifdef __cplusplus
}
#endif

#endif /* VCU_MAIN_H */
