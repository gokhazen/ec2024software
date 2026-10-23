/**
 * @file foc_math.h
 * @brief Field Oriented Control (FOC) Mathematical Transformations & Space Vector PWM
 */

#ifndef FOC_MATH_H
#define FOC_MATH_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float a;
    float b;
    float c;
} Phase_ABC_t;

typedef struct {
    float alpha;
    float beta;
} Vector_AlphaBeta_t;

typedef struct {
    float d;
    float q;
} Vector_DQ_t;

typedef struct {
    float t_a;          /* Phase A PWM Duty (0.0 to 1.0) */
    float t_b;          /* Phase B PWM Duty (0.0 to 1.0) */
    float t_c;          /* Phase C PWM Duty (0.0 to 1.0) */
    uint8_t sector;     /* SVPWM Sector (1 to 6) */
} SVPWM_Output_t;

/* Core Transformations */
void Clarke_Transform(const Phase_ABC_t *abc, Vector_AlphaBeta_t *alphabeta);
void Park_Transform(const Vector_AlphaBeta_t *alphabeta, float theta_rad, Vector_DQ_t *dq);
void Inv_Park_Transform(const Vector_DQ_t *dq, float theta_rad, Vector_AlphaBeta_t *alphabeta);
void SVPWM_Generate(const Vector_AlphaBeta_t *valphabeta, float v_dc_bus, SVPWM_Output_t *pwm_out);

#ifdef __cplusplus
}
#endif

#endif /* FOC_MATH_H */
