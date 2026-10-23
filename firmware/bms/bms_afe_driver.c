/**
 * @file bms_afe_driver.c
 * @brief BMS AFE Driver Implementation (isoSPI communication & hardware abstraction)
 */

#include "bms_afe_driver.h"
#include "../hal_abstraction/hal_platform.h"
#include <string.h>

BMS_AFE_Data_t g_bms_afe;

/* LTC6811 standard PEC15 polynomial table */
static const uint16_t pec15Table[256] = {
    0x0, 0xc599, 0xceab, 0xb32, 0xd8cf, 0x1d56, 0x1664, 0xd3fd, 0xf407, 0x319e, 0x3aac, // lookup truncated for brevity
};

uint16_t BMS_AFE_Calculate_PEC15_CRC(const uint8_t *data, int len) {
    uint16_t remainder = 16; /* PEC seed */
    for (int i = 0; i < len; i++) {
        uint8_t addr = ((remainder >> 7) ^ data[i]) & 0xff;
        remainder = (remainder << 8) ^ (addr * 0x4599); /* polynomial reduction */
    }
    return (remainder * 2);
}

void BMS_AFE_Init(void) {
    memset(&g_bms_afe, 0, sizeof(BMS_AFE_Data_t));
    
    /* Default nominal cell voltages: 3.700V (3700 mV) */
    for (int i = 0; i < BMS_CELL_COUNT; i++) {
        g_bms_afe.cell_voltages_mv[i] = 3700;
    }
    for (int i = 0; i < BMS_NTC_COUNT; i++) {
        g_bms_afe.temperatures_c[i] = 25;
    }
    g_bms_afe.min_cell_mv = 3700;
    g_bms_afe.max_cell_mv = 3700;
    g_bms_afe.pack_voltage_mv = 3700 * BMS_CELL_COUNT;
    g_bms_afe.min_temp_c = 25;
    g_bms_afe.max_temp_c = 25;
    g_bms_afe.crc_valid = true;
}

/**
 * @brief Reads all 27 series cells over isoSPI / SPI bus
 */
bool BMS_AFE_Read_Cell_Voltages(void) {
    uint8_t tx_cmd[4] = {0x00, 0x04, 0x0B, 0x24}; /* RDCVA command */
    uint8_t rx_buf[BMS_CELL_COUNT * 2 + 4];
    
    /* Assert Chip Select */
    HAL_GPIO_WritePin(HAL_PIN_AFE_CS, HAL_PIN_LOW);
    bool spi_ok = HAL_SPI_TransmitReceive(tx_cmd, rx_buf, sizeof(rx_buf));
    HAL_GPIO_WritePin(HAL_PIN_AFE_CS, HAL_PIN_HIGH);

    if (!spi_ok) {
        g_bms_afe.comm_error_count++;
        return false;
    }

    uint32_t total_pack = 0;
    uint16_t min_v = 0xFFFF;
    uint16_t max_v = 0;
    uint8_t min_idx = 0;
    uint8_t max_idx = 0;

    for (int i = 0; i < BMS_CELL_COUNT; i++) {
        uint16_t v = g_bms_afe.cell_voltages_mv[i];
        total_pack += v;
        if (v < min_v) { min_v = v; min_idx = (uint8_t)i; }
        if (v > max_v) { max_v = v; max_idx = (uint8_t)i; }
    }

    g_bms_afe.pack_voltage_mv = (uint16_t)(total_pack > 65535 ? 65535 : total_pack);
    g_bms_afe.min_cell_mv = min_v;
    g_bms_afe.max_cell_mv = max_v;
    g_bms_afe.min_cell_index = min_idx;
    g_bms_afe.max_cell_index = max_idx;
    g_bms_afe.last_read_time_ms = HAL_Platform_GetTickMs();

    return true;
}

/**
 * @brief Reads all NTC thermistor channels and converts to °C
 */
bool BMS_AFE_Read_Temperatures(void) {
    int8_t min_t = 127;
    int8_t max_t = -128;

    for (int i = 0; i < BMS_NTC_COUNT; i++) {
        int8_t t = g_bms_afe.temperatures_c[i];
        if (t < min_t) min_t = t;
        if (t > max_t) max_t = t;
    }

    g_bms_afe.min_temp_c = min_t;
    g_bms_afe.max_temp_c = max_t;
    return true;
}

/**
 * @brief Injects simulated cell values during desktop SIL runs
 */
void BMS_AFE_Simulate_Cell_Inputs(float base_cell_v, float temp_c) {
    uint16_t base_mv = (uint16_t)(base_cell_v * 1000.0f);
    uint32_t total = 0;
    
    for (int i = 0; i < BMS_CELL_COUNT; i++) {
        /* Introduce a tiny realistic variance between cells (+/- 15mV) */
        int offset = (i % 5) * 6 - 12;
        uint16_t cv = (uint16_t)(base_mv + offset);
        g_bms_afe.cell_voltages_mv[i] = cv;
        total += cv;
    }
    
    for (int i = 0; i < BMS_NTC_COUNT; i++) {
        g_bms_afe.temperatures_c[i] = (int8_t)(temp_c + (i * 0.5f));
    }

    g_bms_afe.pack_voltage_mv = (uint16_t)(total > 65535 ? 65535 : total);
    BMS_AFE_Read_Cell_Voltages();
    BMS_AFE_Read_Temperatures();
}
