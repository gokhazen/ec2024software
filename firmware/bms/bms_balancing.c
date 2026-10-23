/**
 * @file bms_balancing.c
 * @brief BMS Passive Cell Balancing Implementation
 */

#include "bms_balancing.h"

BMS_Balancing_t g_bms_balance;

void BMS_Balancing_Init(void) {
    g_bms_balance.balancing_active = false;
    g_bms_balance.active_bleed_mask = 0;
    g_bms_balance.delta_voltage_mv = 0;
    g_bms_balance.balance_threshold_mv = BMS_CELL_BAL_THRESHOLD_MV; /* 50 mV */
    g_bms_balance.cells_balancing_count = 0;
}

/**
 * @brief Updates passive balancing bleeder switches
 * Only balances when charging, battery voltage > 3.4V, and temperature < 45°C
 */
void BMS_Balancing_Update(const uint16_t *cell_voltages_mv, uint16_t min_cell_mv, bool is_charging, int8_t max_temp_c) {
    g_bms_balance.active_bleed_mask = 0;
    g_bms_balance.cells_balancing_count = 0;

    /* Thermal and charging pre-conditions */
    if (!is_charging || max_temp_c > 45 || min_cell_mv < 3400) {
        g_bms_balance.balancing_active = false;
        return;
    }

    uint16_t max_v = 0;
    for (int i = 0; i < BMS_CELL_COUNT; i++) {
        if (cell_voltages_mv[i] > max_v) max_v = cell_voltages_mv[i];
    }
    g_bms_balance.delta_voltage_mv = max_v - min_cell_mv;

    /* If pack spread exceeds 50 mV, activate bleeders */
    if (g_bms_balance.delta_voltage_mv >= g_bms_balance.balance_threshold_mv) {
        g_bms_balance.balancing_active = true;

        for (int i = 0; i < BMS_CELL_COUNT; i++) {
            if ((cell_voltages_mv[i] - min_cell_mv) >= g_bms_balance.balance_threshold_mv) {
                g_bms_balance.active_bleed_mask |= (1UL << i);
                g_bms_balance.cells_balancing_count++;
            }
        }
    } else {
        g_bms_balance.balancing_active = false;
    }
}

bool BMS_IsCellBalancing(uint8_t cell_index) {
    if (cell_index >= BMS_CELL_COUNT) return false;
    return (g_bms_balance.active_bleed_mask & (1UL << cell_index)) != 0;
}
