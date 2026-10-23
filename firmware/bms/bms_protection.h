/**
 * @file bms_protection.h
 * @brief BMS Multi-Tier Protection System and Dynamic Current Derating
 */

#ifndef BMS_PROTECTION_H
#define BMS_PROTECTION_H

#include <stdint.h>
#include <stdbool.h>
#include "../common/ev_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t fault_flags;         /* Bitfield of active BMS faults */
    bool    emergency_cut_req;   /* Request VCU to open contactors immediately */
    
    float   max_allowed_discharge_a; /* Continuous discharge current limit */
    float   max_allowed_charge_a;    /* Continuous charge / regen current limit */
    
    uint32_t ov_start_ms;
    uint32_t uv_start_ms;
    uint32_t ot_start_ms;
} BMS_Protection_t;

extern BMS_Protection_t g_bms_protection;

void BMS_Protection_Init(void);
void BMS_Fault_Monitor_Task(uint16_t min_cell_mv, uint16_t max_cell_mv, int8_t max_temp_c, int8_t min_temp_c, float current_a);
void BMS_Dynamic_Current_Limits(float soc_pct, int8_t max_temp_c);
bool BMS_IsEmergencyStopActive(void);

#ifdef __cplusplus
}
#endif

#endif /* BMS_PROTECTION_H */
