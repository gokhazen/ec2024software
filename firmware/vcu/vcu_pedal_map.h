/**
 * @file vcu_pedal_map.h
 * @brief Dual APPS (Accelerator Pedal Position Sensor) Plausibility & Torque Mapping
 */

#ifndef VCU_PEDAL_MAP_H
#define VCU_PEDAL_MAP_H

#include <stdint.h>
#include <stdbool.h>
#include "../common/ev_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float raw_apps1_norm;        /* 0.0 to 1.0 */
    float raw_apps2_norm;        /* 0.0 to 1.0 */
    float throttle_percent;      /* 0.0 to 100.0 % */
    float brake_percent;         /* 0.0 to 100.0 % */
    
    bool  apps_implausibility;   /* True if channel deviation > 10% */
    bool  brake_override_active; /* True if driver pressed gas + brake simultaneously */
    
    uint32_t implausibility_start_ms;
} Pedal_Controller_t;

extern Pedal_Controller_t g_pedal_ctrl;

void  Pedal_Init(void);
bool  Read_Throttle_ADC(float ch1_norm, float ch2_norm, float *out_throttle_pct);
bool  Brake_Plausibility_Check(float throttle_pct, float brake_pct);
float Calculate_Torque_Request(float throttle_pct, float brake_pct, Drive_Mode_t mode, float max_allowed_torque_nm);

#ifdef __cplusplus
}
#endif

#endif /* VCU_PEDAL_MAP_H */
