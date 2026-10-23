/**
 * @file pid_controller.c
 * @brief Discrete Anti-Windup Clamped PID Controller Implementation
 */

#include "pid_controller.h"

void PID_Init(PID_Controller_t *pid, float Kp, float Ki, float Kd, float Ts, float out_min, float out_max) {
    if (!pid) return;
    
    pid->Kp = Kp;
    pid->Ki = Ki;
    pid->Kd = Kd;
    pid->Ts = Ts;
    pid->out_min = out_min;
    pid->out_max = out_max;
    
    PID_Reset(pid);
}

void PID_Reset(PID_Controller_t *pid) {
    if (!pid) return;
    pid->integral = 0.0f;
    pid->prev_error = 0.0f;
    pid->prev_meas = 0.0f;
    pid->output = 0.0f;
    pid->saturated = 0;
}

float PID_Update(PID_Controller_t *pid, float setpoint, float measurement) {
    if (!pid) return 0.0f;
    
    float error = setpoint - measurement;
    
    /* Proportional term */
    float p_term = pid->Kp * error;
    
    /* Derivative on measurement (prevents derivative kick on setpoint change) */
    float d_term = 0.0f;
    if (pid->Ts > 0.0f) {
        d_term = -pid->Kd * (measurement - pid->prev_meas) / pid->Ts;
    }
    
    /* Tentative integral update */
    float tentative_integral = pid->integral + (pid->Ki * error * pid->Ts);
    
    /* Unconstrained output */
    float raw_output = p_term + tentative_integral + d_term;
    
    /* Anti-windup clamping: only update integral if not saturating in the same direction */
    if (raw_output > pid->out_max) {
        pid->output = pid->out_max;
        pid->saturated = 1;
        /* Only integrate if error acts to decrease output */
        if (error < 0.0f) {
            pid->integral = tentative_integral;
        }
    } else if (raw_output < pid->out_min) {
        pid->output = pid->out_min;
        pid->saturated = -1;
        /* Only integrate if error acts to increase output */
        if (error > 0.0f) {
            pid->integral = tentative_integral;
        }
    } else {
        pid->output = raw_output;
        pid->saturated = 0;
        pid->integral = tentative_integral;
    }
    
    pid->prev_error = error;
    pid->prev_meas = measurement;
    
    return pid->output;
}
