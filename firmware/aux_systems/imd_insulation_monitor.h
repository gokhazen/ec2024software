/**
 * @file imd_insulation_monitor.h
 * @brief Bender IR155 Insulation Monitoring Device (IMD) Interface
 */

#ifndef IMD_INSULATION_MONITOR_H
#define IMD_INSULATION_MONITOR_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    IMD_STATUS_NORMAL = 0,
    IMD_STATUS_UNDERWORK,
    IMD_STATUS_SPEED_START,
    IMD_STATUS_FAULT_CHASSIS,
    IMD_STATUS_HARDWARE_ERROR
} IMD_Condition_t;

typedef struct {
    uint16_t        insulation_resistance_kohm;
    IMD_Condition_t condition;
    bool            critical_fault;            /* True if resistance < 100 kOhm */
    float           pwm_frequency_hz;
    float           pwm_duty_cycle_pct;
    uint32_t        fault_timestamp_ms;
} IMD_Controller_t;

extern IMD_Controller_t g_imd_ctrl;

void IMD_Init(void);
void IMD_Process_PWM(float freq_hz, float duty_pct);
void IMD_SetSimulatedResistance(uint16_t kohm);
bool IMD_IsIsolationSafe(void);

#ifdef __cplusplus
}
#endif

#endif /* IMD_INSULATION_MONITOR_H */
