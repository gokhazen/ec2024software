/**
 * @file foc_current_loop.h
 * @brief High Speed 10 kHz FOC Current Loop Control ISR
 */

#ifndef FOC_CURRENT_LOOP_H
#define FOC_CURRENT_LOOP_H

#include <stdint.h>
#include <stdbool.h>
#include "foc_math.h"
#include "../common/pid_controller.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool              enabled;
    float             dc_bus_voltage_v;
    float             target_torque_nm;
    float             actual_torque_nm;
    
    /* Current Setpoints & Feedback */
    float             target_id_a;
    float             target_iq_a;
    Vector_DQ_t       actual_idq;
    Phase_ABC_t       phase_currents_a;
    Vector_AlphaBeta_t current_alphabeta;
    
    /* PI Controllers */
    PID_Controller_t  pid_id;
    PID_Controller_t  pid_iq;
    
    /* Output Voltages & Duties */
    Vector_DQ_t       target_vdq;
    Vector_AlphaBeta_t target_valphabeta;
    SVPWM_Output_t    pwm_output;
    
    /* Motor Constants */
    float             kt_nm_per_a;   /* Torque constant (Nm/A) */
    float             rs_ohm;        /* Stator resistance */
    float             ls_henry;      /* Stator inductance */
} FOC_Controller_t;

extern FOC_Controller_t g_foc_ctrl;

void FOC_Init(float dc_bus_v);
void FOC_SetTargetTorque(float torque_nm);
void FOC_Enable(bool enable);
void FOC_Current_Control_ISR(float ia, float ib, float theta_rad, float v_dc_bus);

#ifdef __cplusplus
}
#endif

#endif /* FOC_CURRENT_LOOP_H */
