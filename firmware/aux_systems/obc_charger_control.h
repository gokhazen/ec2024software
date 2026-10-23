/**
 * @file obc_charger_control.h
 * @brief On-Board Charger (OBC) SAE J1772 / IEC 62196 Control & CC/CV Profile Manager
 */

#ifndef OBC_CHARGER_CONTROL_H
#define OBC_CHARGER_CONTROL_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    OBC_STATE_IDLE = 0,
    OBC_STATE_CONNECTED_WAIT_PILOT,
    OBC_STATE_PRECHARGE,
    OBC_STATE_CONSTANT_CURRENT,
    OBC_STATE_CONSTANT_VOLTAGE,
    OBC_STATE_CHARGE_COMPLETE,
    OBC_STATE_FAULT
} OBC_State_t;

typedef struct {
    OBC_State_t state;
    bool        plug_detected;          /* Proximity Pilot (PP) connected */
    bool        drive_lockout_active;   /* Strictly inhibit vehicle drive */
    
    float       pilot_duty_cycle_pct;   /* J1772 Control Pilot PWM (0..100%) */
    float       grid_max_current_a;     /* Calculated max allowable current */
    
    float       output_voltage_v;       /* Charging output voltage */
    float       output_current_a;       /* Charging output current */
    float       target_current_a;
    float       target_voltage_v;       /* Pack CV voltage (e.g. 27 * 4.2V = 113.4V) */
    
    uint32_t    charge_start_time_ms;
    uint32_t    energy_delivered_wh;
} OBC_Controller_t;

extern OBC_Controller_t g_obc_ctrl;

void OBC_Init(float pack_cv_target_voltage_v);
void OBC_Process_Pilot_PWM(float duty_cycle_pct, bool proximity_plug_present);
void OBC_Task_50ms_Loop(float pack_voltage_v, float bms_allowed_charge_current_a, float max_cell_mv);
const char* OBC_StateToString(OBC_State_t state);

#ifdef __cplusplus
}
#endif

#endif /* OBC_CHARGER_CONTROL_H */
