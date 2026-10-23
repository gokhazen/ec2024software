/**
 * @file vcu_safety_precharge.h
 * @brief High Voltage Contactor Sequencing and Precharge Safety Manager
 */

#ifndef VCU_SAFETY_PRECHARGE_H
#define VCU_SAFETY_PRECHARGE_H

#include <stdint.h>
#include <stdbool.h>
#include "../common/ev_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PRECHARGE_IDLE = 0,
    PRECHARGE_CLOSE_NEG,
    PRECHARGE_CLOSE_PRECHARGE_RELAY,
    PRECHARGE_WAIT_VOLTAGE,
    PRECHARGE_CLOSE_POS,
    PRECHARGE_OPEN_PRECHARGE_RELAY,
    PRECHARGE_COMPLETE,
    PRECHARGE_FAILED
} Precharge_Substate_t;

typedef struct {
    Precharge_Substate_t substate;
    uint32_t             start_time_ms;
    uint32_t             elapsed_ms;
    float                initial_battery_voltage_v;
    float                current_dc_bus_voltage_v;
    float                voltage_ratio;
    bool                 main_neg_closed;
    bool                 precharge_relay_closed;
    bool                 main_pos_closed;
} Precharge_Controller_t;

extern Precharge_Controller_t g_precharge_ctrl;

void Precharge_Init(void);
void Precharge_Start(float battery_voltage_v);
bool Precharge_Process_Run(float dc_bus_voltage_v, uint32_t current_time_ms);
void Contactor_Emergency_Open(const char *reason);
void Contactor_Normal_Shutdown(void);
bool Precharge_IsComplete(void);

#ifdef __cplusplus
}
#endif

#endif /* VCU_SAFETY_PRECHARGE_H */
