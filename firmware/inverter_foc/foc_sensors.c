/**
 * @file foc_sensors.c
 * @brief Rotor Position & Speed Sensing Implementation
 */

#include "foc_sensors.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

Rotor_Sensor_t g_rotor_sensor;

void Rotor_Sensor_Init(uint8_t pole_pairs) {
    g_rotor_sensor.pole_pairs = (pole_pairs > 0) ? pole_pairs : 4;
    g_rotor_sensor.electrical_angle_rad = 0.0f;
    g_rotor_sensor.electrical_angle_deg = 0.0f;
    g_rotor_sensor.mechanical_angle_rad = 0.0f;
    g_rotor_sensor.rotor_speed_rpm = 0.0f;
    g_rotor_sensor.raw_sin = 0.0f;
    g_rotor_sensor.raw_cos = 1.0f;
}

/**
 * @brief Updates position from tracking resolver Demodulated Sin/Cos signals
 */
void Rotor_Sensor_Update_Resolver(float sin_sample, float cos_sample, float dt_seconds) {
    g_rotor_sensor.raw_sin = sin_sample;
    g_rotor_sensor.raw_cos = cos_sample;

    /* Four-quadrant arctangent yields mechanical rotor angle */
    float mech_angle = atan2f(sin_sample, cos_sample);
    if (mech_angle < 0.0f) {
        mech_angle += 2.0f * M_PI;
    }

    /* Electrical angle = PolePairs * Mechanical Angle (modulo 2*PI) */
    float elec_angle = fmodf(mech_angle * (float)g_rotor_sensor.pole_pairs, 2.0f * M_PI);
    if (elec_angle < 0.0f) elec_angle += 2.0f * M_PI;

    /* Speed calculation (dTheta / dt) with low-pass filter */
    float delta_mech = mech_angle - g_rotor_sensor.mechanical_angle_rad;
    if (delta_mech > M_PI) delta_mech -= 2.0f * M_PI;
    else if (delta_mech < -M_PI) delta_mech += 2.0f * M_PI;

    if (dt_seconds > 0.0f) {
        float inst_rpm = (delta_mech / (2.0f * M_PI)) * (60.0f / dt_seconds);
        /* 1st order filter */
        g_rotor_sensor.rotor_speed_rpm = (0.85f * g_rotor_sensor.rotor_speed_rpm) + (0.15f * inst_rpm);
    }

    g_rotor_sensor.mechanical_angle_rad = mech_angle;
    g_rotor_sensor.electrical_angle_rad = elec_angle;
    g_rotor_sensor.electrical_angle_deg = (elec_angle * 180.0f) / M_PI;
}

void Rotor_Sensor_Update_Encoder(uint32_t encoder_counts, uint32_t counts_per_rev, float dt_seconds) {
    if (counts_per_rev == 0) return;
    float norm = (float)(encoder_counts % counts_per_rev) / (float)counts_per_rev;
    float sin_val = sinf(norm * 2.0f * M_PI);
    float cos_val = cosf(norm * 2.0f * M_PI);
    Rotor_Sensor_Update_Resolver(sin_val, cos_val, dt_seconds);
}

float Rotor_Sensor_GetElectricalAngle(void) {
    return g_rotor_sensor.electrical_angle_rad;
}

float Rotor_Sensor_GetSpeedRPM(void) {
    return g_rotor_sensor.rotor_speed_rpm;
}
