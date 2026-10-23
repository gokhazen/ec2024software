/**
 * @file bms_afe_driver.h
 * @brief Battery Management System Analog Front End (AFE) Driver (LTC6811/BQ76952)
 */

#ifndef BMS_AFE_DRIVER_H
#define BMS_AFE_DRIVER_H

#include <stdint.h>
#include <stdbool.h>
#include "../common/ev_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t cell_voltages_mv[BMS_CELL_COUNT];
    int8_t   temperatures_c[BMS_NTC_COUNT];
    
    uint16_t pack_voltage_mv;
    uint16_t min_cell_mv;
    uint16_t max_cell_mv;
    uint8_t  min_cell_index;
    uint8_t  max_cell_index;
    
    int8_t   min_temp_c;
    int8_t   max_temp_c;
    
    uint32_t last_read_time_ms;
    uint32_t comm_error_count;
    bool     crc_valid;
} BMS_AFE_Data_t;

extern BMS_AFE_Data_t g_bms_afe;

void BMS_AFE_Init(void);
bool BMS_AFE_Read_Cell_Voltages(void);
bool BMS_AFE_Read_Temperatures(void);
void BMS_AFE_Simulate_Cell_Inputs(float base_cell_v, float temp_c);
uint16_t BMS_AFE_Calculate_PEC15_CRC(const uint8_t *data, int len);

#ifdef __cplusplus
}
#endif

#endif /* BMS_AFE_DRIVER_H */
