/**
 * @file foc_current_loop.c
 * @brief High-Speed 10 kHz (100 µs) FOC Current Loop Control Implementation
 */

#include "foc_current_loop.h"
#include <math.h>

FOC_Controller_t g_foc_ctrl;

#define FOC_LOOP_PERIOD_S  0.0001f /* 100 microseconds (10 kHz) */
#define SQRT3_INV          0.577350269f

void FOC_Init(float dc_bus_v) {
    g_foc_ctrl.enabled = false;
    g_foc_ctrl.dc_bus_voltage_v = dc_bus_v;
    g_foc_ctrl.target_torque_nm = 0.0f;
    g_foc_ctrl.actual_torque_nm = 0.0f;
    g_foc_ctrl.target_id_a = 0.0f;
    g_foc_ctrl.target_iq_a = 0.0f;
    
    g_foc_ctrl.kt_nm_per_a = 1.25f;  /* 1.25 Nm per Ampere */
    g_foc_ctrl.rs_ohm = 0.05f;       /* 50 mOhm */
    g_foc_ctrl.ls_henry = 0.00025f;  /* 250 uH */

    float max_voltage = (dc_bus_v > 10.0f) ? (dc_bus_v * SQRT3_INV) : 50.0f;

    /* Initialize PI Controllers for D and Q axis:
     * Kp = Ls / Tau, Ki = Rs / Tau (with Tau = 1ms current loop time constant) */
    PID_Init(&g_foc_ctrl.pid_id, 0.25f, 50.0f, 0.0f, FOC_LOOP_PERIOD_S, -max_voltage, max_voltage);
    PID_Init(&g_foc_ctrl.pid_iq, 0.25f, 50.0f, 0.0f, FOC_LOOP_PERIOD_S, -max_voltage, max_voltage);
}

void FOC_SetTargetTorque(float torque_nm) {
    g_foc_ctrl.target_torque_nm = torque_nm;
    /* Compute desired Iq from torque constant: T = 1.5 * p * lambda_pm * Iq = Kt * Iq */
    if (g_foc_ctrl.kt_nm_per_a > 0.01f) {
        g_foc_ctrl.target_iq_a = torque_nm / g_foc_ctrl.kt_nm_per_a;
    } else {
        g_foc_ctrl.target_iq_a = 0.0f;
    }
    /* Id remains 0 for non-salient PMSM under base speed */
    g_foc_ctrl.target_id_a = 0.0f;
}

void FOC_Enable(bool enable) {
    g_foc_ctrl.enabled = enable;
    if (!enable) {
        PID_Reset(&g_foc_ctrl.pid_id);
        PID_Reset(&g_foc_ctrl.pid_iq);
        g_foc_ctrl.target_torque_nm = 0.0f;
        g_foc_ctrl.target_iq_a = 0.0f;
        g_foc_ctrl.target_id_a = 0.0f;
        g_foc_ctrl.pwm_output.t_a = 0.5f;
        g_foc_ctrl.pwm_output.t_b = 0.5f;
        g_foc_ctrl.pwm_output.t_c = 0.5f;
    }
}

/**
 * @brief 10 kHz Hardware Timer ISR entry point
 */
void FOC_Current_Control_ISR(float ia, float ib, float theta_rad, float v_dc_bus) {
    g_foc_ctrl.dc_bus_voltage_v = v_dc_bus;

    if (!g_foc_ctrl.enabled || v_dc_bus < 10.0f) {
        g_foc_ctrl.pwm_output.t_a = 0.5f;
        g_foc_ctrl.pwm_output.t_b = 0.5f;
        g_foc_ctrl.pwm_output.t_c = 0.5f;
        g_foc_ctrl.actual_torque_nm = 0.0f;
        return;
    }

    /* 1. Reconstruct Phase C current (Kirchhoff: Ia + Ib + Ic = 0) */
    g_foc_ctrl.phase_currents_a.a = ia;
    g_foc_ctrl.phase_currents_a.b = ib;
    g_foc_ctrl.phase_currents_a.c = -(ia + ib);

    /* 2. Forward Clarke Transformation */
    Clarke_Transform(&g_foc_ctrl.phase_currents_a, &g_foc_ctrl.current_alphabeta);

    /* 3. Forward Park Transformation */
    Park_Transform(&g_foc_ctrl.current_alphabeta, theta_rad, &g_foc_ctrl.actual_idq);

    /* Actual torque feedback */
    g_foc_ctrl.actual_torque_nm = g_foc_ctrl.actual_idq.q * g_foc_ctrl.kt_nm_per_a;

    /* 4. PI Regulators on current errors */
    float vd = PID_Update(&g_foc_ctrl.pid_id, g_foc_ctrl.target_id_a, g_foc_ctrl.actual_idq.d);
    float vq = PID_Update(&g_foc_ctrl.pid_iq, g_foc_ctrl.target_iq_a, g_foc_ctrl.actual_idq.q);

    /* Vector saturation to maximum voltage circle (V_dc / sqrt(3)) */
    float max_v = v_dc_bus * SQRT3_INV;
    float v_mag = sqrtf(vd * vd + vq * vq);
    if (v_mag > max_v) {
        float scale = max_v / v_mag;
        vd *= scale;
        vq *= scale;
    }

    g_foc_ctrl.target_vdq.d = vd;
    g_foc_ctrl.target_vdq.q = vq;

    /* 5. Inverse Park Transformation */
    Inv_Park_Transform(&g_foc_ctrl.target_vdq, theta_rad, &g_foc_ctrl.target_valphabeta);

    /* 6. Space Vector PWM Generation */
    SVPWM_Generate(&g_foc_ctrl.target_valphabeta, v_dc_bus, &g_foc_ctrl.pwm_output);
}
