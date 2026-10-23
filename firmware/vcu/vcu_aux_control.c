/**
 * @file vcu_aux_control.c
 * @brief Thermal Management (Pump, Fan) and Low-Voltage Aux Control
 */

#include "vcu_aux_control.h"
#include "../hal_abstraction/hal_platform.h"

Aux_Controller_t g_aux_ctrl;

void Aux_Control_Init(void) {
    g_aux_ctrl.cooling_pump_pwm_pct = 0.0f;
    g_aux_ctrl.radiator_fan_pwm_pct = 0.0f;
    g_aux_ctrl.headlights_active = false;
    g_aux_ctrl.brakelights_active = false;
    g_aux_ctrl.wipers_active = false;
    g_aux_ctrl.max_measured_temp_c = 25.0f;

    HAL_PWM_SetDuty(HAL_PIN_COOLING_PUMP_EN, 0.0f);
    HAL_PWM_SetDuty(HAL_PIN_RADIATOR_FAN_EN, 0.0f);
    HAL_GPIO_WritePin(HAL_PIN_BRAKE_LIGHT, HAL_PIN_LOW);
    HAL_GPIO_WritePin(HAL_PIN_HEADLIGHT, HAL_PIN_LOW);
}

/**
 * @brief Dynamic thermal management loop
 * Proportional coolant pump speed + multi-stage radiator fan
 */
void Aux_Thermal_Management_Task(float motor_temp_c, float inv_temp_c, float bms_temp_c) {
    /* Determine highest heat source */
    float max_temp = motor_temp_c;
    if (inv_temp_c > max_temp) max_temp = inv_temp_c;
    if (bms_temp_c > max_temp) max_temp = bms_temp_c;
    g_aux_ctrl.max_measured_temp_c = max_temp;

    /* Coolant Pump Logic:
     * < 35°C : 20% minimum circulation
     * 35°C - 60°C : linear ramp from 20% to 100%
     * > 60°C : 100% full flow
     */
    if (max_temp < 35.0f) {
        g_aux_ctrl.cooling_pump_pwm_pct = 20.0f;
    } else if (max_temp >= 60.0f) {
        g_aux_ctrl.cooling_pump_pwm_pct = 100.0f;
    } else {
        g_aux_ctrl.cooling_pump_pwm_pct = 20.0f + ((max_temp - 35.0f) / (60.0f - 35.0f)) * 80.0f;
    }

    /* Radiator Fan Logic:
     * < 45°C : Fan OFF
     * 45°C - 55°C : Stage 1 (50% PWM)
     * > 55°C : Stage 2 (100% PWM)
     */
    if (max_temp < 45.0f) {
        g_aux_ctrl.radiator_fan_pwm_pct = 0.0f;
    } else if (max_temp < 55.0f) {
        g_aux_ctrl.radiator_fan_pwm_pct = 50.0f;
    } else {
        g_aux_ctrl.radiator_fan_pwm_pct = 100.0f;
    }

    HAL_PWM_SetDuty(HAL_PIN_COOLING_PUMP_EN, g_aux_ctrl.cooling_pump_pwm_pct);
    HAL_PWM_SetDuty(HAL_PIN_RADIATOR_FAN_EN, g_aux_ctrl.radiator_fan_pwm_pct);
}

void Aux_SetLighting(bool headlights, bool brake_pressed) {
    g_aux_ctrl.headlights_active = headlights;
    g_aux_ctrl.brakelights_active = brake_pressed;

    HAL_GPIO_WritePin(HAL_PIN_HEADLIGHT, headlights ? HAL_PIN_HIGH : HAL_PIN_LOW);
    HAL_GPIO_WritePin(HAL_PIN_BRAKE_LIGHT, brake_pressed ? HAL_PIN_HIGH : HAL_PIN_LOW);
}
