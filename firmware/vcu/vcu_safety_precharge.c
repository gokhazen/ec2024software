/**
 * @file vcu_safety_precharge.c
 * @brief Precharge sequence management and emergency contactor safety cutoff
 */

#include "vcu_safety_precharge.h"
#include "../hal_abstraction/hal_platform.h"
#include <stdio.h>

Precharge_Controller_t g_precharge_ctrl;

void Precharge_Init(void) {
    g_precharge_ctrl.substate = PRECHARGE_IDLE;
    g_precharge_ctrl.start_time_ms = 0;
    g_precharge_ctrl.elapsed_ms = 0;
    g_precharge_ctrl.initial_battery_voltage_v = 0.0f;
    g_precharge_ctrl.current_dc_bus_voltage_v = 0.0f;
    g_precharge_ctrl.voltage_ratio = 0.0f;
    g_precharge_ctrl.main_neg_closed = false;
    g_precharge_ctrl.precharge_relay_closed = false;
    g_precharge_ctrl.main_pos_closed = false;

    /* Ensure all physical contactors are open on boot */
    HAL_GPIO_WritePin(HAL_PIN_CONTACTOR_MAIN_NEG, HAL_PIN_LOW);
    HAL_GPIO_WritePin(HAL_PIN_CONTACTOR_PRECHARGE, HAL_PIN_LOW);
    HAL_GPIO_WritePin(HAL_PIN_CONTACTOR_MAIN_POS, HAL_PIN_LOW);
}

void Precharge_Start(float battery_voltage_v) {
    g_precharge_ctrl.initial_battery_voltage_v = battery_voltage_v;
    g_precharge_ctrl.start_time_ms = HAL_Platform_GetTickMs();
    g_precharge_ctrl.elapsed_ms = 0;
    g_precharge_ctrl.substate = PRECHARGE_CLOSE_NEG;
}

/**
 * @brief Periodic precharge state executor (called in 10ms task)
 * @return true if precharge completed successfully, false if in progress or failed
 */
bool Precharge_Process_Run(float dc_bus_voltage_v, uint32_t current_time_ms) {
    g_precharge_ctrl.current_dc_bus_voltage_v = dc_bus_voltage_v;
    if (g_precharge_ctrl.initial_battery_voltage_v > 10.0f) {
        g_precharge_ctrl.voltage_ratio = dc_bus_voltage_v / g_precharge_ctrl.initial_battery_voltage_v;
    } else {
        g_precharge_ctrl.voltage_ratio = 0.0f;
    }

    if (g_precharge_ctrl.substate != PRECHARGE_IDLE && 
        g_precharge_ctrl.substate != PRECHARGE_COMPLETE &&
        g_precharge_ctrl.substate != PRECHARGE_FAILED) {
        g_precharge_ctrl.elapsed_ms = current_time_ms - g_precharge_ctrl.start_time_ms;
        
        /* 3000ms Timeout enforcement */
        if (g_precharge_ctrl.elapsed_ms > PRECHARGE_TIMEOUT_MS) {
            Contactor_Emergency_Open("Precharge Timeout Exceeded (3000ms)");
            g_precharge_ctrl.substate = PRECHARGE_FAILED;
            return false;
        }
    }

    switch (g_precharge_ctrl.substate) {
        case PRECHARGE_IDLE:
            break;

        case PRECHARGE_CLOSE_NEG:
            /* Step 1: Close Main Negative Contactor */
            HAL_GPIO_WritePin(HAL_PIN_CONTACTOR_MAIN_NEG, HAL_PIN_HIGH);
            g_precharge_ctrl.main_neg_closed = true;
            g_precharge_ctrl.substate = PRECHARGE_CLOSE_PRECHARGE_RELAY;
            break;

        case PRECHARGE_CLOSE_PRECHARGE_RELAY:
            /* Step 2: Close Precharge Relay (current flows through precharge resistor) */
            HAL_GPIO_WritePin(HAL_PIN_CONTACTOR_PRECHARGE, HAL_PIN_HIGH);
            g_precharge_ctrl.precharge_relay_closed = true;
            g_precharge_ctrl.substate = PRECHARGE_WAIT_VOLTAGE;
            break;

        case PRECHARGE_WAIT_VOLTAGE:
            /* Step 3: Wait until DC Bus reaches >= 95% of Battery Voltage */
            if (g_precharge_ctrl.voltage_ratio >= PRECHARGE_TARGET_RATIO) {
                g_precharge_ctrl.substate = PRECHARGE_CLOSE_POS;
            }
            break;

        case PRECHARGE_CLOSE_POS:
            /* Step 4: Close Main Positive Contactor (DC bus now solidly energized) */
            HAL_GPIO_WritePin(HAL_PIN_CONTACTOR_MAIN_POS, HAL_PIN_HIGH);
            g_precharge_ctrl.main_pos_closed = true;
            g_precharge_ctrl.substate = PRECHARGE_OPEN_PRECHARGE_RELAY;
            break;

        case PRECHARGE_OPEN_PRECHARGE_RELAY:
            /* Step 5: Open Precharge Relay to stop heating precharge resistor */
            HAL_GPIO_WritePin(HAL_PIN_CONTACTOR_PRECHARGE, HAL_PIN_LOW);
            g_precharge_ctrl.precharge_relay_closed = false;
            g_precharge_ctrl.substate = PRECHARGE_COMPLETE;
            return true;

        case PRECHARGE_COMPLETE:
            return true;

        case PRECHARGE_FAILED:
        default:
            return false;
    }

    return (g_precharge_ctrl.substate == PRECHARGE_COMPLETE);
}

/**
 * @brief Immediate Hardware & Software Contactors Opening (< 10ms execution)
 */
void Contactor_Emergency_Open(const char *reason) {
    /* Immediately drop all gate drive signals for contactors */
    HAL_GPIO_WritePin(HAL_PIN_CONTACTOR_MAIN_POS, HAL_PIN_LOW);
    HAL_GPIO_WritePin(HAL_PIN_CONTACTOR_PRECHARGE, HAL_PIN_LOW);
    HAL_GPIO_WritePin(HAL_PIN_CONTACTOR_MAIN_NEG, HAL_PIN_LOW);

    g_precharge_ctrl.main_pos_closed = false;
    g_precharge_ctrl.precharge_relay_closed = false;
    g_precharge_ctrl.main_neg_closed = false;
    g_precharge_ctrl.substate = PRECHARGE_FAILED;

    (void)reason; /* Logged or used in CAN diagnostic DTC */
}

void Contactor_Normal_Shutdown(void) {
    /* Safe ordered sequence */
    HAL_GPIO_WritePin(HAL_PIN_CONTACTOR_MAIN_POS, HAL_PIN_LOW);
    HAL_GPIO_WritePin(HAL_PIN_CONTACTOR_PRECHARGE, HAL_PIN_LOW);
    HAL_Platform_DelayMs(20);
    HAL_GPIO_WritePin(HAL_PIN_CONTACTOR_MAIN_NEG, HAL_PIN_LOW);

    g_precharge_ctrl.main_pos_closed = false;
    g_precharge_ctrl.precharge_relay_closed = false;
    g_precharge_ctrl.main_neg_closed = false;
    g_precharge_ctrl.substate = PRECHARGE_IDLE;
}

bool Precharge_IsComplete(void) {
    return (g_precharge_ctrl.substate == PRECHARGE_COMPLETE);
}
