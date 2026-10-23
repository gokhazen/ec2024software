/**
 * @file imd_insulation_monitor.c
 * @brief Bender IR155 Insulation Monitor Signal Analysis Implementation
 */

#include "imd_insulation_monitor.h"
#include "../vcu/vcu_safety_precharge.h"
#include "../hal_abstraction/hal_platform.h"

IMD_Controller_t g_imd_ctrl;

#define IMD_CRITICAL_THRESHOLD_KOHM 100

void IMD_Init(void) {
    g_imd_ctrl.insulation_resistance_kohm = 2500; /* Healthy 2.5 MOhm default */
    g_imd_ctrl.condition = IMD_STATUS_NORMAL;
    g_imd_ctrl.critical_fault = false;
    g_imd_ctrl.pwm_frequency_hz = 10.0f;
    g_imd_ctrl.pwm_duty_cycle_pct = 50.0f;
    g_imd_ctrl.fault_timestamp_ms = 0;
}

/**
 * @brief Decodes Bender IR155 PWM waveform
 * Frequency specifies operating condition:
 * - 10 Hz: Normal insulation measurement mode
 * - 20 Hz: Underwork / calibration
 * - 30 Hz: Device internal fault / earth fault
 * - 40 Hz: Speed start measurement
 * Duty cycle (5%..95%) translates into measured resistance
 */
void IMD_Process_PWM(float freq_hz, float duty_pct) {
    g_imd_ctrl.pwm_frequency_hz = freq_hz;
    g_imd_ctrl.pwm_duty_cycle_pct = duty_pct;

    if (freq_hz >= 9.0f && freq_hz <= 11.0f) {
        /* Normal measurement mode */
        g_imd_ctrl.condition = IMD_STATUS_NORMAL;

        /* Bender IR155 Standard Formula:
         * Rf = (90% * 1200 kOhm) / (duty% - 5%) - 1200 kOhm */
        if (duty_pct > 5.0f && duty_pct < 95.0f) {
            float r_kohm = (1080.0f / (duty_pct - 5.0f)) - 12.0f;
            if (r_kohm < 0.0f) r_kohm = 0.0f;
            g_imd_ctrl.insulation_resistance_kohm = (uint16_t)r_kohm;
        } else if (duty_pct <= 5.0f) {
            g_imd_ctrl.insulation_resistance_kohm = 0;
        } else {
            g_imd_ctrl.insulation_resistance_kohm = 10000;
        }
    } else if (freq_hz >= 28.0f && freq_hz <= 32.0f) {
        g_imd_ctrl.condition = IMD_STATUS_FAULT_CHASSIS;
        g_imd_ctrl.insulation_resistance_kohm = 0;
    } else {
        g_imd_ctrl.condition = IMD_STATUS_HARDWARE_ERROR;
    }

    /* Check Critical Safety Threshold (< 100 kOhm) */
    if (g_imd_ctrl.insulation_resistance_kohm < IMD_CRITICAL_THRESHOLD_KOHM || 
        g_imd_ctrl.condition == IMD_STATUS_FAULT_CHASSIS) {
        
        if (!g_imd_ctrl.critical_fault) {
            g_imd_ctrl.critical_fault = true;
            g_imd_ctrl.fault_timestamp_ms = HAL_Platform_GetTickMs();
            /* Immediate hardware trigger to open HV contactors */
            Contactor_Emergency_Open("Critical IMD Isolation Fault (<100 kOhm)");
        }
    } else {
        g_imd_ctrl.critical_fault = false;
    }
}

void IMD_SetSimulatedResistance(uint16_t kohm) {
    g_imd_ctrl.insulation_resistance_kohm = kohm;
    if (kohm < IMD_CRITICAL_THRESHOLD_KOHM) {
        g_imd_ctrl.critical_fault = true;
        g_imd_ctrl.condition = IMD_STATUS_FAULT_CHASSIS;
        Contactor_Emergency_Open("Simulated Isolation Breakdown");
    } else {
        g_imd_ctrl.critical_fault = false;
        g_imd_ctrl.condition = IMD_STATUS_NORMAL;
    }
}

bool IMD_IsIsolationSafe(void) {
    return (!g_imd_ctrl.critical_fault && g_imd_ctrl.insulation_resistance_kohm >= IMD_CRITICAL_THRESHOLD_KOHM);
}
