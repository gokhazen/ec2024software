/**
 * @file hal_platform.h
 * @brief Unified Hardware Abstraction Layer (HAL) for STM32 and Desktop SIL Simulation
 */

#ifndef HAL_PLATFORM_H
#define HAL_PLATFORM_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * CAN Frame Structure for HAL
 * ========================================================================= */
typedef struct {
    uint32_t id;         /* 11-bit standard or 29-bit extended ID */
    uint8_t  is_extended;/* 0: Standard, 1: Extended */
    uint8_t  dlc;        /* 0..8 bytes */
    uint8_t  data[8];    /* Payload */
} HAL_CAN_Message_t;

typedef void (*HAL_CAN_RxCallback_t)(const HAL_CAN_Message_t *msg);

/* =========================================================================
 * GPIO Definitions
 * ========================================================================= */
typedef enum {
    HAL_PIN_CONTACTOR_MAIN_NEG = 0,
    HAL_PIN_CONTACTOR_PRECHARGE,
    HAL_PIN_CONTACTOR_MAIN_POS,
    HAL_PIN_COOLING_PUMP_EN,
    HAL_PIN_RADIATOR_FAN_EN,
    HAL_PIN_BRAKE_LIGHT,
    HAL_PIN_HEADLIGHT,
    HAL_PIN_EMERGENCY_STOP_SENSE,
    HAL_PIN_IMD_FAULT_SENSE,
    HAL_PIN_AFE_CS,
    HAL_PIN_COUNT
} HAL_Pin_t;

typedef enum {
    HAL_PIN_LOW = 0,
    HAL_PIN_HIGH = 1
} HAL_PinState_t;

/* =========================================================================
 * Function Prototypes
 * ========================================================================= */

/* System Time */
uint32_t HAL_Platform_GetTickMs(void);
void     HAL_Platform_DelayMs(uint32_t ms);

/* GPIO */
void           HAL_GPIO_WritePin(HAL_Pin_t pin, HAL_PinState_t state);
HAL_PinState_t HAL_GPIO_ReadPin(HAL_Pin_t pin);
void           HAL_GPIO_TogglePin(HAL_Pin_t pin);

/* CAN Interface */
bool HAL_CAN_Transmit(const HAL_CAN_Message_t *msg);
void HAL_CAN_SetRxCallback(HAL_CAN_RxCallback_t callback);

/* PWM Interface (0..100% duty cycle) */
void HAL_PWM_SetDuty(HAL_Pin_t pin, float duty_pct);

/* ADC Interface (Returns normalized 0.0f .. 1.0f or direct mV) */
float    HAL_ADC_ReadNormalized(uint8_t channel);
uint16_t HAL_ADC_ReadMilliVolts(uint8_t channel);

/* SPI Interface for AFE / LTC6811 / BQ76952 */
bool HAL_SPI_TransmitReceive(const uint8_t *tx_data, uint8_t *rx_data, size_t length);

#ifdef __cplusplus
}
#endif

#endif /* HAL_PLATFORM_H */
