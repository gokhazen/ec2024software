/**
 * @file abs_traction_control.h
 * @brief ABS (Anti-Lock Braking) & TCS (Traction Control System) via Motor Regen & Torque Modulation
 */

#ifndef ABS_TRACTION_CONTROL_H
#define ABS_TRACTION_CONTROL_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float vehicle_speed_mps;      /* Longitudinal estimated vehicle speed (m/s) */
    float wheel_speed_mps;        /* Driven wheel peripheral speed (m/s) */
    float slip_ratio;             /* Longitudinal slip ratio (0.0 to 1.0) */
    
    bool  abs_active;             /* True when ABS is pulsing regen */
    bool  tcs_active;             /* True when TCS is clipping drive torque */
    
    float tcs_torque_reduction;   /* 0.0 (no cut) to 1.0 (100% cut) */
    uint32_t abs_pulse_counter;
} ABS_TCS_Controller_t;

extern ABS_TCS_Controller_t g_abs_tcs;

void  ABS_TCS_Init(void);
float Calculate_Wheel_Slip(float vehicle_speed_mps, float wheel_speed_mps, bool is_braking);
float ABS_Regen_Modulator(float raw_regen_torque_nm, float slip_ratio);
float TCS_Torque_Limit(float requested_drive_torque_nm, float slip_ratio);

#ifdef __cplusplus
}
#endif

#endif /* ABS_TRACTION_CONTROL_H */
