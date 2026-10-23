/**
 * @file bms_protection.c
 * @brief BMS Multi-Tier Protection and Dynamic Derating Implementation
 */

#include "bms_protection.h"
#include "../hal_abstraction/hal_platform.h"

BMS_Protection_t g_bms_protection;

#define NOMINAL_MAX_DISCHARGE_A 250.0f
#define NOMINAL_MAX_CHARGE_A    100.0f

void BMS_Protection_Init(void) {
    g_bms_protection.fault_flags = BMS_FAULT_NONE;
    g_bms_protection.emergency_cut_req = false;
    g_bms_protection.max_allowed_discharge_a = NOMINAL_MAX_DISCHARGE_A;
    g_bms_protection.max_allowed_charge_a = NOMINAL_MAX_CHARGE_A;
    g_bms_protection.ov_start_ms = 0;
    g_bms_protection.uv_start_ms = 0;
    g_bms_protection.ot_start_ms = 0;
}

/**
 * @brief Main 20ms Protection Task
 * Checks limits with de-bounce filtering to prevent false trips on noise
 */
void BMS_Fault_Monitor_Task(uint16_t min_cell_mv, uint16_t max_cell_mv, int8_t max_temp_c, int8_t min_temp_c, float current_a) {
    uint32_t now = HAL_Platform_GetTickMs();

    /* 1. Over-Voltage Protection (> 4220 mV) */
    if (max_cell_mv >= BMS_CELL_MAX_VOLTAGE_MV) {
        if (g_bms_protection.ov_start_ms == 0) g_bms_protection.ov_start_ms = now;
        if (now - g_bms_protection.ov_start_ms > 200) { /* 200ms debounce */
            g_bms_protection.fault_flags |= BMS_FAULT_OVER_VOLT;
            g_bms_protection.emergency_cut_req = true;
        }
    } else {
        g_bms_protection.ov_start_ms = 0;
    }

    /* 2. Under-Voltage Protection (< 2500 mV) */
    if (min_cell_mv <= BMS_CELL_MIN_VOLTAGE_MV) {
        if (g_bms_protection.uv_start_ms == 0) g_bms_protection.uv_start_ms = now;
        if (now - g_bms_protection.uv_start_ms > 200) {
            g_bms_protection.fault_flags |= BMS_FAULT_UNDER_VOLT;
            g_bms_protection.emergency_cut_req = true;
        }
    } else {
        g_bms_protection.uv_start_ms = 0;
    }

    /* 3. Over-Temperature Protection (> 60 °C) */
    if (max_temp_c >= BMS_MAX_TEMPERATURE_C) {
        if (g_bms_protection.ot_start_ms == 0) g_bms_protection.ot_start_ms = now;
        if (now - g_bms_protection.ot_start_ms > 500) {
            g_bms_protection.fault_flags |= BMS_FAULT_OVER_TEMP;
            g_bms_protection.emergency_cut_req = true;
        }
    } else {
        g_bms_protection.ot_start_ms = 0;
    }

    /* 4. Under-Temperature Protection (<-10 °C during charging) */
    if (min_temp_c < BMS_MIN_TEMPERATURE_C) {
        g_bms_protection.fault_flags |= BMS_FAULT_UNDER_TEMP;
    }

    /* 5. Over-Current Protection (> 300A for > 100ms) */
    if (current_a > 300.0f || current_a < -150.0f) {
        g_bms_protection.fault_flags |= BMS_FAULT_OVER_CURRENT;
        g_bms_protection.emergency_cut_req = true;
    }
}

/**
 * @brief Dynamic Current Derating Calculation
 * Prevents battery aging and thermal runaway by scaling back current limits
 */
void BMS_Dynamic_Current_Limits(float soc_pct, int8_t max_temp_c) {
    float discharge_limit = NOMINAL_MAX_DISCHARGE_A;
    float charge_limit = NOMINAL_MAX_CHARGE_A;

    /* Thermal Derating (Discharge & Charge) */
    if (max_temp_c >= 55) {
        /* Linearly derate from 55°C to 60°C down to 0 */
        float factor = 1.0f - ((float)(max_temp_c - 55) / 5.0f);
        if (factor < 0.0f) factor = 0.0f;
        discharge_limit *= factor;
        charge_limit *= factor;
    } else if (max_temp_c < 0) {
        /* Cold weather derating: prevent lithium plating */
        charge_limit *= 0.20f;
    }

    /* Low SOC Derating (Discharge) */
    if (soc_pct < 20.0f) {
        float factor = soc_pct / 20.0f;
        discharge_limit *= factor;
    }

    /* High SOC Derating (Charge / Regen) */
    if (soc_pct > 80.0f) {
        float factor = 1.0f - ((soc_pct - 80.0f) / 20.0f);
        if (factor < 0.0f) factor = 0.0f;
        charge_limit *= factor;
    }

    g_bms_protection.max_allowed_discharge_a = discharge_limit;
    g_bms_protection.max_allowed_charge_a = charge_limit;
}

bool BMS_IsEmergencyStopActive(void) {
    return g_bms_protection.emergency_cut_req;
}
