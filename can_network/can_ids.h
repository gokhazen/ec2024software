/**
 * @file can_ids.h
 * @brief CAN Matrix IDs, Frame Structures, and Bitfields for Electric Vehicle Software
 * @note Follows CAN 2.0B Standard (500 kbps)
 */

#ifndef CAN_IDS_H
#define CAN_IDS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * CAN Message IDs (11-bit Standard Identifiers)
 * ========================================================================= */
#define CAN_ID_VCU_HEARTBEAT_STATE    0x100  /* 10ms - VCU State & Drive Mode */
#define CAN_ID_VCU_TORQUE_COMMAND     0x101  /* 10ms - Target Torque & Inv Enable */
#define CAN_ID_VCU_AUX_STATUS         0x102  /* 100ms - Pump, Fan, Contactors */

#define CAN_ID_BMS_PACK_STATUS        0x200  /* 20ms - Pack V, I, SOC, SOH */
#define CAN_ID_BMS_LIMITS_ALARMS      0x201  /* 50ms - Dynamic Current Limits & Alarms */
#define CAN_ID_BMS_CELL_EXTREMES      0x202  /* 100ms - Min/Max Cell mV, Min/Max Temp */

#define CAN_ID_INV_MOTOR_FEEDBACK     0x300  /* 10ms - RPM, Actual Torque, DC Bus */
#define CAN_ID_INV_THERMAL_STATUS     0x301  /* 100ms - Inverter & Motor Temp */
#define CAN_ID_ABS_TCS_STATUS         0x302  /* 20ms - Slip %, ABS/TCS active */

#define CAN_ID_OBC_STATUS_CONTROL     0x400  /* 50ms - AC plug, pilot limit, V_out, I_out */
#define CAN_ID_IMD_STATUS             0x450  /* 100ms - Bender IR155 Isolation kOhm */

/* =========================================================================
 * Message Payloads (Packed C Structures - 8 Bytes each)
 * ========================================================================= */
#pragma pack(push, 1)

typedef struct {
    uint8_t state;               /* 0:INIT, 1:STANDBY, 2:PRECHARGE, 3:DRIVE, 4:FAULT */
    uint16_t fault_code;         /* Bitfield of system faults */
    uint8_t drive_mode;          /* 0:ECO, 1:NORMAL, 2:SPORT */
    uint8_t rolling_counter;     /* Alive counter 0..255 */
    uint8_t flags;               /* bit 0: brake_pressed, bit 1: ready_to_drive */
    uint16_t reserved;
} CAN_Msg_VCU_Heartbeat_t;

typedef struct {
    int16_t target_torque_0_1Nm; /* Scale: 0.1 Nm (-5000 to +5000 -> -500.0 to +500.0 Nm) */
    uint8_t control_flags;       /* bit 0: enable, bit 1: dir_fwd, bit 2: regen_allowed */
    uint8_t reserved1;
    uint16_t max_speed_rpm;      /* Max allowed RPM limiter */
    uint16_t reserved2;
} CAN_Msg_VCU_TorqueCommand_t;

typedef struct {
    uint8_t cooling_pump_pwm;    /* 0..100 % */
    uint8_t radiator_fan_pwm;    /* 0..100 % */
    uint8_t contactors_lights;   /* bit 0: head, bit 1: brake, bit 2: precharge, bit 3: pos, bit 4: neg */
    uint8_t reserved[5];
} CAN_Msg_VCU_AuxStatus_t;

typedef struct {
    uint16_t pack_voltage_0_1V;  /* Scale: 0.1 V (e.g. 1000 = 100.0 V) */
    int16_t pack_current_0_1A;   /* Scale: 0.1 A (-500.0A to +500.0A, + is discharge, - is charge) */
    uint8_t soc_0_5pct;          /* Scale: 0.5 % (0..200 -> 0.0..100.0 %) */
    uint8_t soh_0_5pct;          /* Scale: 0.5 % (0..200 -> 0.0..100.0 %) */
    uint8_t contactor_req_fault; /* bit 0: contactor_req, bits 1..3: fault_level */
    uint8_t reserved;
} CAN_Msg_BMS_PackStatus_t;

typedef struct {
    uint16_t max_discharge_0_1A; /* Dynamic discharge limit */
    uint16_t max_charge_0_1A;    /* Dynamic charge limit */
    uint8_t alarm_flags;         /* bit0: OV, bit1: UV, bit2: OT, bit3: UT, bit4: Balancing */
    uint8_t reserved[3];
} CAN_Msg_BMS_LimitsAlarms_t;

typedef struct {
    uint16_t cell_min_mV;        /* Minimum cell voltage in mV */
    uint16_t cell_max_mV;        /* Maximum cell voltage in mV */
    int8_t temp_min_c;           /* Minimum cell temp in °C */
    int8_t temp_max_c;           /* Maximum cell temp in °C */
    uint8_t cell_min_id;         /* Cell index (1..27) */
    uint8_t cell_max_id;         /* Cell index (1..27) */
} CAN_Msg_BMS_CellExtremes_t;

typedef struct {
    int16_t motor_speed_rpm;     /* -12000 to +12000 RPM */
    int16_t actual_torque_0_1Nm; /* -500.0 to +500.0 Nm */
    uint16_t dc_bus_voltage_0_1V;/* 0.0 to 600.0 V */
    int16_t dc_bus_current_0_1A; /* -500.0 to +500.0 A */
} CAN_Msg_INV_Feedback_t;

typedef struct {
    int8_t inverter_temp_c;      /* Inverter heatsink / IGBT temp */
    int8_t motor_temp_c;         /* Motor winding temp */
    uint16_t fault_flags;        /* Inverter internal faults */
    uint8_t derating_active;     /* 1 if derating due to thermal/voltage */
    uint8_t reserved[3];
} CAN_Msg_INV_Thermal_t;

typedef struct {
    uint8_t slip_ratio_0_5pct;   /* Slip ratio: 0..200 (0..100%) */
    uint8_t flags;               /* bit 0: abs_active, bit 1: tcs_active */
    uint8_t torque_reduction_pct;/* 0..100% torque reduction applied */
    uint8_t reserved[5];
} CAN_Msg_ABS_TCS_Status_t;

typedef struct {
    uint8_t ac_present;          /* 1 if AC mains plugged in */
    uint8_t pilot_current_limit; /* Amps (e.g. 16A or 32A) */
    uint16_t output_voltage_0_1V;/* Actual charging voltage */
    uint16_t output_current_0_1A;/* Actual charging current */
    uint8_t charging_state;      /* 0: IDLE, 1: PRECHARGE, 2: CC, 3: CV, 4: COMPLETE, 5: FAULT */
    uint8_t reserved;
} CAN_Msg_OBC_Status_t;

typedef struct {
    uint16_t insulation_kOhm;    /* Measured insulation resistance */
    uint8_t status_flag;         /* Bender IR155 state */
    uint8_t isolation_fault;     /* 1 if below 100 kOhm critical threshold */
    uint8_t reserved[4];
} CAN_Msg_IMD_Status_t;

#pragma pack(pop)

#ifdef __cplusplus
}
#endif

#endif /* CAN_IDS_H */
