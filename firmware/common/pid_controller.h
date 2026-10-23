/**
 * @file pid_controller.h
 * @brief Discrete Anti-Windup PID Controller for FOC, Thermal, and Speed Loops
 */

#ifndef PID_CONTROLLER_H
#define PID_CONTROLLER_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float Kp;           /* Proportional Gain */
    float Ki;           /* Integral Gain */
    float Kd;           /* Derivative Gain */
    float Ts;           /* Sampling Time (seconds) */
    
    float out_min;      /* Minimum output limit */
    float out_max;      /* Maximum output limit */
    
    float integral;     /* Integrated error state */
    float prev_error;   /* Previous error for derivative */
    float prev_meas;    /* Previous measurement for derivative-on-measurement */
    
    float output;       /* Clamped output */
    int   saturated;    /* 1 if output is clamped */
} PID_Controller_t;

void PID_Init(PID_Controller_t *pid, float Kp, float Ki, float Kd, float Ts, float out_min, float out_max);
void PID_Reset(PID_Controller_t *pid);
float PID_Update(PID_Controller_t *pid, float setpoint, float measurement);

#ifdef __cplusplus
}
#endif

#endif /* PID_CONTROLLER_H */
