/**
 * @file vcu_pedal_map.c
 * @brief Dual APPS (Accelerator Pedal Position Sensor) Plausibility & Torque Mapping
 */

#include "vcu_pedal_map.h"
#include "../hal_abstraction/hal_platform.h"
#include <math.h>

Pedal_Controller_t g_pedal_ctrl;

#define APPS_PLAUSIBILITY_MAX_DEV  0.10f   /* 10% maximum deviation */
#define APPS_TIMEOUT_MS            100     /* 100ms maximum persistence before fault */
#define APPS_DEADZONE_PCT          3.0f    /* 3% deadzone at rest */
#define BRAKE_OVERRIDE_THROTTLE_TH 25.0f   /* 25% gas pedal threshold for brake override */
#define BRAKE_ACTIVE_THRESHOLD_PCT 5.0f    /* 5% brake pedal triggers brake light and plausibility */

void Pedal_Init(void) {
    g_pedal_ctrl.raw_apps1_norm = 0.0f;
    g_pedal_ctrl.raw_apps2_norm = 0.0f;
    g_pedal_ctrl.throttle_percent = 0.0f;
    g_pedal_ctrl.brake_percent = 0.0f;
    g_pedal_ctrl.apps_implausibility = false;
    g_pedal_ctrl.brake_override_active = false;
    g_pedal_ctrl.implausibility_start_ms = 0;
}

/**
 * @brief Reads redundant dual-channel throttle sensor with cross-validation
 */
bool Read_Throttle_ADC(float ch1_norm, float ch2_norm, float *out_throttle_pct) {
    g_pedal_ctrl.raw_apps1_norm = ch1_norm;
    g_pedal_ctrl.raw_apps2_norm = ch2_norm;

    /* Out of bounds hardware check (short-circuit / open-circuit detection) */
    if (ch1_norm < 0.02f || ch1_norm > 0.98f || ch2_norm < 0.02f || ch2_norm > 0.98f) {
        g_pedal_ctrl.apps_implausibility = true;
        *out_throttle_pct = 0.0f;
        return false;
    }

    /* Redundancy check: |ch1 - ch2| must be <= 10% */
    float deviation = fabsf(ch1_norm - ch2_norm);
    uint32_t now = HAL_Platform_GetTickMs();

    if (deviation > APPS_PLAUSIBILITY_MAX_DEV) {
        if (g_pedal_ctrl.implausibility_start_ms == 0) {
            g_pedal_ctrl.implausibility_start_ms = now;
        } else if (now - g_pedal_ctrl.implausibility_start_ms > APPS_TIMEOUT_MS) {
            g_pedal_ctrl.apps_implausibility = true;
            *out_throttle_pct = 0.0f;
            return false;
        }
    } else {
        g_pedal_ctrl.implausibility_start_ms = 0;
        g_pedal_ctrl.apps_implausibility = false;
    }

    /* Compute average and apply deadzone */
    float avg = (ch1_norm + ch2_norm) * 0.5f;
    float pct = (avg - 0.05f) / (0.90f - 0.05f) * 100.0f;
    
    if (pct < APPS_DEADZONE_PCT) {
        pct = 0.0f;
    } else if (pct > 100.0f) {
        pct = 100.0f;
    }

    g_pedal_ctrl.throttle_percent = pct;
    *out_throttle_pct = pct;
    return true;
}

/**
 * @brief Brake plausibility / Brake Override Algorithm (ISO 26262 & FSAE standard)
 * If brake is pressed while throttle > 25%, torque is immediately cut to 0
 * and remains 0 until throttle drops below 5%.
 */
bool Brake_Plausibility_Check(float throttle_pct, float brake_pct) {
    g_pedal_ctrl.brake_percent = brake_pct;

    if (brake_pct > BRAKE_ACTIVE_THRESHOLD_PCT && throttle_pct > BRAKE_OVERRIDE_THROTTLE_TH) {
        g_pedal_ctrl.brake_override_active = true;
    } else if (g_pedal_ctrl.brake_override_active) {
        if (throttle_pct < 5.0f) {
            /* Driver released the gas pedal; unlatch safety override */
            g_pedal_ctrl.brake_override_active = false;
        }
    }

    return g_pedal_ctrl.brake_override_active;
}

/**
 * @brief Calculates torque request based on driving mode, pedal position, and derating
 */
float Calculate_Torque_Request(float throttle_pct, float brake_pct, Drive_Mode_t mode, float max_allowed_torque_nm) {
    /* If safety checks fail, strictly output 0.0 Nm */
    if (g_pedal_ctrl.apps_implausibility || g_pedal_ctrl.brake_override_active) {
        return 0.0f;
    }

    /* If brake is applied, generate regenerative braking torque */
    if (brake_pct > BRAKE_ACTIVE_THRESHOLD_PCT) {
        float regen_factor = (brake_pct - BRAKE_ACTIVE_THRESHOLD_PCT) / (100.0f - BRAKE_ACTIVE_THRESHOLD_PCT);
        float regen_torque = -1.0f * regen_factor * MAX_REGEN_TORQUE_NM;
        return regen_torque;
    }

    float norm_throttle = throttle_pct / 100.0f;
    float torque = 0.0f;

    switch (mode) {
        case DRIVE_MODE_ECO:
            /* Quadratic progressive curve for energy saving (up to 120 Nm) */
            torque = (norm_throttle * norm_throttle) * 120.0f;
            break;

        case DRIVE_MODE_SPORT:
            /* Aggressive initial response (sqrt-like) up to full vehicle limit */
            torque = sqrtf(norm_throttle) * MAX_MOTOR_TORQUE_NM;
            break;

        case DRIVE_MODE_NORMAL:
        default:
            /* Linear response up to 180 Nm */
            torque = norm_throttle * 180.0f;
            break;
    }

    /* Enforce dynamic BMS / Inverter derating limit */
    if (torque > max_allowed_torque_nm) {
        torque = max_allowed_torque_nm;
    }
    if (torque < -MAX_REGEN_TORQUE_NM) {
        torque = -MAX_REGEN_TORQUE_NM;
    }

    return torque;
}
