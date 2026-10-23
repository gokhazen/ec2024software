/**
 * @file ev_types.h
 * @brief Common types, constants, state machines, and thresholds for EV Firmware
 */

#ifndef EV_TYPES_H
#define EV_TYPES_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * VCU Master State Machine
 * ========================================================================= */
typedef enum {
    VCU_STATE_INIT = 0,
    VCU_STATE_STANDBY,
    VCU_STATE_PRECHARGE,
    VCU_STATE_READY_TO_DRIVE,
    VCU_STATE_FAULT
} VCU_State_t;

typedef enum {
    DRIVE_MODE_ECO = 0,
    DRIVE_MODE_NORMAL,
    DRIVE_MODE_SPORT
} Drive_Mode_t;

typedef enum {
    VCU_FAULT_NONE              = 0x0000,
    VCU_FAULT_PRECHARGE_TIMEOUT = 0x0001,
    VCU_FAULT_APPS_PLAUSIBILITY = 0x0002,
    VCU_FAULT_BMS_EMERGENCY     = 0x0004,
    VCU_FAULT_IMD_ISOLATION     = 0x0008,
    VCU_FAULT_INVERTER_ERROR    = 0x0010,
    VCU_FAULT_CAN_TIMEOUT       = 0x0020,
    VCU_FAULT_EMERGENCY_STOP    = 0x0040
} VCU_FaultCode_t;

/* =========================================================================
 * BMS States and Faults
 * ========================================================================= */
#define BMS_CELL_COUNT              27    /* 27S Battery Pack */
#define BMS_NTC_COUNT               8     /* 8 Temperature Sensor Channels */

#define BMS_CELL_MAX_VOLTAGE_MV     4220  /* 4.22V Over-Voltage Protection */
#define BMS_CELL_MIN_VOLTAGE_MV     2500  /* 2.50V Under-Voltage Protection */
#define BMS_CELL_BAL_THRESHOLD_MV   50    /* 50mV Passive Balancing Delta */
#define BMS_MAX_TEMPERATURE_C      60    /* 60°C Over-Temperature Protection */
#define BMS_MIN_TEMPERATURE_C      -10   /* -10°C Low Temperature Charge Cutoff */

typedef enum {
    BMS_FAULT_NONE         = 0x00,
    BMS_FAULT_OVER_VOLT    = 0x01,
    BMS_FAULT_UNDER_VOLT   = 0x02,
    BMS_FAULT_OVER_TEMP    = 0x04,
    BMS_FAULT_UNDER_TEMP   = 0x08,
    BMS_FAULT_OVER_CURRENT = 0x10,
    BMS_FAULT_COMMUNICATION= 0x20
} BMS_Fault_t;

/* =========================================================================
 * Safety & Precharge Thresholds
 * ========================================================================= */
#define PRECHARGE_TARGET_RATIO      0.95f /* 95% of battery voltage required */
#define PRECHARGE_TIMEOUT_MS        3000  /* Maximum 3000ms allowed */
#define IMD_CRITICAL_RESISTANCE_KOHM 100  /* Below 100 kOhm triggers emergency open */

/* =========================================================================
 * Inverter / FOC Parameters
 * ========================================================================= */
#define MOTOR_POLE_PAIRS            4
#define PWM_FREQUENCY_HZ            10000 /* 10 kHz Current Loop ISR */
#define MAX_MOTOR_TORQUE_NM         250.0f
#define MAX_REGEN_TORQUE_NM         100.0f
#define ABS_SLIP_TRIGGER_RATIO      0.20f /* 20% slip threshold for ABS */

/* Standard Return / Status Enum */
typedef enum {
    EV_OK = 0,
    EV_ERROR,
    EV_TIMEOUT,
    EV_BUSY,
    EV_INVALID_ARG
} EV_Status_t;

#ifdef __cplusplus
}
#endif

#endif /* EV_TYPES_H */
