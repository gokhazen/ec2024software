/**
 * @file bms_balancing.h
 * @brief BMS Passive Cell Balancing Controller
 */

#ifndef BMS_BALANCING_H
#define BMS_BALANCING_H

#include <stdint.h>
#include <stdbool.h>
#include "../common/ev_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool    balancing_active;
    uint32_t active_bleed_mask;   /* Bits 0..26 indicate which cells have bleeder active */
    uint16_t delta_voltage_mv;
    uint16_t balance_threshold_mv;
    uint8_t  cells_balancing_count;
} BMS_Balancing_t;

extern BMS_Balancing_t g_bms_balance;

void BMS_Balancing_Init(void);
void BMS_Balancing_Update(const uint16_t *cell_voltages_mv, uint16_t min_cell_mv, bool is_charging, int8_t max_temp_c);
bool BMS_IsCellBalancing(uint8_t cell_index);

#ifdef __cplusplus
}
#endif

#endif /* BMS_BALANCING_H */
