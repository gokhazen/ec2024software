/**
 * @file foc_sensors.h
 * @brief Rotor Position & Speed Sensing (Resolver / Incremental Encoder)
 */

#ifndef FOC_SENSORS_H
#define FOC_SENSORS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float electrical_angle_rad;   /* 0.0 to 2*PI radians */
    float electrical_angle_deg;   /* 0.0 to 360.0 degrees */
    float mechanical_angle_rad;   /* 0.0 to 2*PI radians */
    float rotor_speed_rpm;        /* -12000 to +12000 RPM */
    float raw_sin;
    float raw_cos;
    uint8_t pole_pairs;
} Rotor_Sensor_t;

extern Rotor_Sensor_t g_rotor_sensor;

void  Rotor_Sensor_Init(uint8_t pole_pairs);
void  Rotor_Sensor_Update_Resolver(float sin_sample, float cos_sample, float dt_seconds);
void  Rotor_Sensor_Update_Encoder(uint32_t encoder_counts, uint32_t counts_per_rev, float dt_seconds);
float Rotor_Sensor_GetElectricalAngle(void);
float Rotor_Sensor_GetSpeedRPM(void);

#ifdef __cplusplus
}
#endif

#endif /* FOC_SENSORS_H */
