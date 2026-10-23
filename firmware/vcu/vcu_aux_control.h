/**
 * @file vcu_aux_control.h
 * @brief Thermal Management (Pump, Fan) and Low-Voltage Aux Control
 */

#ifndef VCU_AUX_CONTROL_H
#define VCU_AUX_CONTROL_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float cooling_pump_pwm_pct;  /* 0..100 % */
    float radiator_fan_pwm_pct;  /* 0..100 % */
    bool  headlights_active;
    bool  brakelights_active;
    bool  wipers_active;
    
    float max_measured_temp_c;   /* Highest between motor, inverter, battery */
} Aux_Controller_t;

extern Aux_Controller_t g_aux_ctrl;

void Aux_Control_Init(void);
void Aux_Thermal_Management_Task(float motor_temp_c, float inv_temp_c, float bms_temp_c);
void Aux_SetLighting(bool headlights, bool brake_pressed);

#ifdef __cplusplus
}
#endif

#endif /* VCU_AUX_CONTROL_H */
