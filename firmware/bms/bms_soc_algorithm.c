/**
 * @file bms_soc_algorithm.c
 * @brief Coulomb Counting + Extended Kalman Filter (EKF) SOC Implementation
 */

#include "bms_soc_algorithm.h"
#include <math.h>

BMS_SOC_Estimator_t g_bms_soc;

/* Typical Lithium-Ion NMC Open Circuit Voltage (OCV) Table per cell */
typedef struct {
    float soc;  /* 0.0 to 1.0 */
    float ocv;  /* Volts */
} OCV_Point_t;

static const OCV_Point_t ocv_table[] = {
    {0.00f, 3.00f},
    {0.10f, 3.45f},
    {0.20f, 3.60f},
    {0.30f, 3.68f},
    {0.40f, 3.74f},
    {0.50f, 3.80f},
    {0.60f, 3.87f},
    {0.70f, 3.95f},
    {0.80f, 4.04f},
    {0.90f, 4.13f},
    {1.00f, 4.20f}
};
#define OCV_TABLE_LEN (sizeof(ocv_table) / sizeof(ocv_table[0]))

void BMS_SOC_Init(float nominal_capacity_ah, float initial_soc) {
    g_bms_soc.nominal_capacity_ah = nominal_capacity_ah;
    g_bms_soc.soc_percent = initial_soc;
    g_bms_soc.soh_percent = 100.0f;
    g_bms_soc.remaining_capacity_ah = (initial_soc / 100.0f) * nominal_capacity_ah;
    
    g_bms_soc.ekf_x_soc = initial_soc / 100.0f;
    g_bms_soc.ekf_p_covariance = 0.01f;
    g_bms_soc.ekf_q_process_noise = 0.0001f;
    g_bms_soc.ekf_r_meas_noise = 0.02f;
    g_bms_soc.last_calc_time_ms = 0;
}

float BMS_Get_OCV_From_SOC(float soc) {
    if (soc <= 0.0f) return ocv_table[0].ocv;
    if (soc >= 1.0f) return ocv_table[OCV_TABLE_LEN - 1].ocv;

    for (size_t i = 0; i < OCV_TABLE_LEN - 1; i++) {
        if (soc >= ocv_table[i].soc && soc <= ocv_table[i+1].soc) {
            float frac = (soc - ocv_table[i].soc) / (ocv_table[i+1].soc - ocv_table[i].soc);
            return ocv_table[i].ocv + frac * (ocv_table[i+1].ocv - ocv_table[i].ocv);
        }
    }
    return 3.70f;
}

float BMS_Get_SOC_From_OCV(float ocv) {
    if (ocv <= ocv_table[0].ocv) return 0.0f;
    if (ocv >= ocv_table[OCV_TABLE_LEN - 1].ocv) return 1.0f;

    for (size_t i = 0; i < OCV_TABLE_LEN - 1; i++) {
        if (ocv >= ocv_table[i].ocv && ocv <= ocv_table[i+1].ocv) {
            float frac = (ocv - ocv_table[i].ocv) / (ocv_table[i+1].ocv - ocv_table[i].ocv);
            return ocv_table[i].soc + frac * (ocv_table[i+1].soc - ocv_table[i].soc);
        }
    }
    return 0.5f;
}

/**
 * @brief Discrete Coulomb Counting step: Q = Q0 - (I * dt)
 */
float BMS_SOC_CoulombCounting(float current_a, float dt_seconds) {
    /* Positive current is discharge, negative is charge */
    float delta_ah = (current_a * dt_seconds) / 3600.0f;
    g_bms_soc.remaining_capacity_ah -= delta_ah;

    if (g_bms_soc.remaining_capacity_ah < 0.0f) {
        g_bms_soc.remaining_capacity_ah = 0.0f;
    } else if (g_bms_soc.remaining_capacity_ah > g_bms_soc.nominal_capacity_ah) {
        g_bms_soc.remaining_capacity_ah = g_bms_soc.nominal_capacity_ah;
    }

    g_bms_soc.soc_percent = (g_bms_soc.remaining_capacity_ah / g_bms_soc.nominal_capacity_ah) * 100.0f;
    g_bms_soc.ekf_x_soc = g_bms_soc.soc_percent / 100.0f;
    return g_bms_soc.soc_percent;
}

/**
 * @brief Extended Kalman Filter (EKF) measurement update
 * Corrects Coulomb counting drift using cell Open Circuit Voltage
 */
float BMS_SOC_EKF_Correction(float ocv_cell_voltage_v, float current_a) {
    /* 1. Time Update (Prediction) */
    /* x_prior = x_prev (already updated by Coulomb counter) */
    float p_prior = g_bms_soc.ekf_p_covariance + g_bms_soc.ekf_q_process_noise;

    /* 2. Measurement Prediction: V_model = OCV(x) - I * R_internal */
    float internal_r = 0.003f; /* 3 mOhm per cell typical */
    float v_pred = BMS_Get_OCV_From_SOC(g_bms_soc.ekf_x_soc) - (current_a * internal_r);

    /* Derivative d(OCV)/d(SOC) at current operating point */
    float delta = 0.01f;
    float h = (BMS_Get_OCV_From_SOC(g_bms_soc.ekf_x_soc + delta) - BMS_Get_OCV_From_SOC(g_bms_soc.ekf_x_soc)) / delta;

    /* 3. Kalman Gain */
    float s = h * p_prior * h + g_bms_soc.ekf_r_meas_noise;
    float k = (p_prior * h) / s;

    /* 4. Measurement Update (Correction) */
    float error = ocv_cell_voltage_v - v_pred;
    g_bms_soc.ekf_x_soc += k * error;
    g_bms_soc.ekf_p_covariance = (1.0f - k * h) * p_prior;

    /* Bound estimated SOC between 0% and 100% */
    if (g_bms_soc.ekf_x_soc < 0.0f) g_bms_soc.ekf_x_soc = 0.0f;
    if (g_bms_soc.ekf_x_soc > 1.0f) g_bms_soc.ekf_x_soc = 1.0f;

    g_bms_soc.soc_percent = g_bms_soc.ekf_x_soc * 100.0f;
    g_bms_soc.remaining_capacity_ah = (g_bms_soc.soc_percent / 100.0f) * g_bms_soc.nominal_capacity_ah;

    return g_bms_soc.soc_percent;
}
