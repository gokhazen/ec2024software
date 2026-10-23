/**
 * @file bms_soc_algorithm.h
 * @brief State of Charge (SOC) via Coulomb Counting & Extended Kalman Filter (EKF)
 */

#ifndef BMS_SOC_ALGORITHM_H
#define BMS_SOC_ALGORITHM_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float soc_percent;          /* 0.0 to 100.0 % */
    float soh_percent;          /* 0.0 to 100.0 % */
    float nominal_capacity_ah;  /* Rated pack capacity (e.g. 50.0 Ah) */
    float remaining_capacity_ah;/* Current remaining capacity */
    
    /* EKF States */
    float ekf_x_soc;            /* State: estimated SOC [0..1] */
    float ekf_p_covariance;     /* Error covariance */
    float ekf_q_process_noise;  /* Process noise */
    float ekf_r_meas_noise;     /* Measurement noise */
    
    uint32_t last_calc_time_ms;
} BMS_SOC_Estimator_t;

extern BMS_SOC_Estimator_t g_bms_soc;

void  BMS_SOC_Init(float nominal_capacity_ah, float initial_soc);
float BMS_SOC_CoulombCounting(float current_a, float dt_seconds);
float BMS_SOC_EKF_Correction(float ocv_cell_voltage_v, float current_a);
float BMS_Get_OCV_From_SOC(float soc_0_to_1);
float BMS_Get_SOC_From_OCV(float ocv_cell_v);

#ifdef __cplusplus
}
#endif

#endif /* BMS_SOC_ALGORITHM_H */
