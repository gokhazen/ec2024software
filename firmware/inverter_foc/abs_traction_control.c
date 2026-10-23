/**
 * @file abs_traction_control.c
 * @brief ABS and TCS Slip-based Active Safety Algorithm Implementation
 */

#include "abs_traction_control.h"
#include <math.h>

ABS_TCS_Controller_t g_abs_tcs;

#define ABS_SLIP_LOCK_THRESHOLD  0.20f /* 20% slip triggers ABS */
#define TCS_SLIP_SPIN_THRESHOLD  0.15f /* 15% slip triggers TCS */

void ABS_TCS_Init(void) {
    g_abs_tcs.vehicle_speed_mps = 0.0f;
    g_abs_tcs.wheel_speed_mps = 0.0f;
    g_abs_tcs.slip_ratio = 0.0f;
    g_abs_tcs.abs_active = false;
    g_abs_tcs.tcs_active = false;
    g_abs_tcs.tcs_torque_reduction = 0.0f;
    g_abs_tcs.abs_pulse_counter = 0;
}

/**
 * @brief Calculates normalized longitudinal slip ratio
 */
float Calculate_Wheel_Slip(float vehicle_speed_mps, float wheel_speed_mps, bool is_braking) {
    g_abs_tcs.vehicle_speed_mps = vehicle_speed_mps;
    g_abs_tcs.wheel_speed_mps = wheel_speed_mps;

    /* Avoid division by zero at standstill */
    if (vehicle_speed_mps < 1.0f && wheel_speed_mps < 1.0f) {
        g_abs_tcs.slip_ratio = 0.0f;
        return 0.0f;
    }

    float slip = 0.0f;
    if (is_braking) {
        /* Braking slip: (V_veh - V_wheel) / V_veh */
        if (vehicle_speed_mps > 0.1f) {
            slip = (vehicle_speed_mps - wheel_speed_mps) / vehicle_speed_mps;
        }
    } else {
        /* Acceleration slip: (V_wheel - V_veh) / V_wheel */
        if (wheel_speed_mps > 0.1f) {
            slip = (wheel_speed_mps - vehicle_speed_mps) / wheel_speed_mps;
        }
    }

    if (slip < 0.0f) slip = 0.0f;
    if (slip > 1.0f) slip = 1.0f;

    g_abs_tcs.slip_ratio = slip;
    return slip;
}

/**
 * @brief ABS Regenerative Braking Modulator
 * If slip > 20%, drops regenerative braking to 0 instantly and pulses back
 * to maintain maximum tire mu peak without wheel lockup
 */
float ABS_Regen_Modulator(float raw_regen_torque_nm, float slip_ratio) {
    /* Regenerative braking torque is negative */
    if (raw_regen_torque_nm >= 0.0f) {
        g_abs_tcs.abs_active = false;
        return raw_regen_torque_nm;
    }

    if (slip_ratio >= ABS_SLIP_LOCK_THRESHOLD) {
        g_abs_tcs.abs_active = true;
        g_abs_tcs.abs_pulse_counter++;

        /* 10Hz square pulse cycle: 20ms OFF, 20ms 50% modulation */
        if ((g_abs_tcs.abs_pulse_counter % 4) < 2) {
            /* Release brake torque completely to let wheel spin up */
            return 0.0f;
        } else {
            /* Apply soft modulated torque */
            return raw_regen_torque_nm * 0.40f;
        }
    } else {
        g_abs_tcs.abs_active = false;
        g_abs_tcs.abs_pulse_counter = 0;
        return raw_regen_torque_nm;
    }
}

/**
 * @brief Traction Control System (TCS) Torque Limiter
 * Progressively clips drive torque when tire spinning is detected
 */
float TCS_Torque_Limit(float requested_drive_torque_nm, float slip_ratio) {
    if (requested_drive_torque_nm <= 0.0f) {
        g_abs_tcs.tcs_active = false;
        g_abs_tcs.tcs_torque_reduction = 0.0f;
        return requested_drive_torque_nm;
    }

    if (slip_ratio >= TCS_SLIP_SPIN_THRESHOLD) {
        g_abs_tcs.tcs_active = true;
        /* Proportional cut based on slip excess */
        float excess = slip_ratio - TCS_SLIP_SPIN_THRESHOLD;
        float cut_ratio = excess * 4.0f; /* 25% slip -> 40% torque cut */
        if (cut_ratio > 0.85f) cut_ratio = 0.85f; /* Do not stall completely */

        g_abs_tcs.tcs_torque_reduction = cut_ratio;
        return requested_drive_torque_nm * (1.0f - cut_ratio);
    } else {
        g_abs_tcs.tcs_active = false;
        g_abs_tcs.tcs_torque_reduction = 0.0f;
        return requested_drive_torque_nm;
    }
}
