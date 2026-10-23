/**
 * @file foc_math.c
 * @brief FOC Mathematical Transformations and SVPWM Generation
 */

#include "foc_math.h"
#include <math.h>

#define ONE_BY_SQRT3  0.577350269f
#define SQRT3         1.732050808f
#define TWO_BY_SQRT3  1.154700538f

/**
 * @brief Forward Clarke Transformation (3-phase a,b,c to 2-phase alpha, beta)
 */
void Clarke_Transform(const Phase_ABC_t *abc, Vector_AlphaBeta_t *alphabeta) {
    if (!abc || !alphabeta) return;
    alphabeta->alpha = abc->a;
    alphabeta->beta  = (abc->a + 2.0f * abc->b) * ONE_BY_SQRT3;
}

/**
 * @brief Forward Park Transformation (alpha, beta to rotating d, q)
 */
void Park_Transform(const Vector_AlphaBeta_t *alphabeta, float theta_rad, Vector_DQ_t *dq) {
    if (!alphabeta || !dq) return;
    float cos_t = cosf(theta_rad);
    float sin_t = sinf(theta_rad);

    dq->d =  alphabeta->alpha * cos_t + alphabeta->beta * sin_t;
    dq->q = -alphabeta->alpha * sin_t + alphabeta->beta * cos_t;
}

/**
 * @brief Inverse Park Transformation (rotating d, q to stationary alpha, beta)
 */
void Inv_Park_Transform(const Vector_DQ_t *dq, float theta_rad, Vector_AlphaBeta_t *alphabeta) {
    if (!dq || !alphabeta) return;
    float cos_t = cosf(theta_rad);
    float sin_t = sinf(theta_rad);

    alphabeta->alpha = dq->d * cos_t - dq->q * sin_t;
    alphabeta->beta  = dq->d * sin_t + dq->q * cos_t;
}

/**
 * @brief Space Vector PWM (SVPWM) Generator
 * Computes dwell times T1, T2, T0 and maps to 3-phase center-aligned duties
 */
void SVPWM_Generate(const Vector_AlphaBeta_t *valphabeta, float v_dc_bus, SVPWM_Output_t *pwm_out) {
    if (!valphabeta || !pwm_out || v_dc_bus <= 1.0f) return;

    /* Normalize target voltages by DC bus voltage */
    float u_alpha = valphabeta->alpha / v_dc_bus;
    float u_beta  = valphabeta->beta  / v_dc_bus;

    /* Intermediate reference projections */
    float u1 = u_beta;
    float u2 = (-u_beta + SQRT3 * u_alpha) * 0.5f;
    float u3 = (-u_beta - SQRT3 * u_alpha) * 0.5f;

    /* Sector identification */
    uint8_t sector = 0;
    if (u1 > 0.0f) sector += 1;
    if (u2 > 0.0f) sector += 2;
    if (u3 > 0.0f) sector += 4;

    float t1 = 0.0f;
    float t2 = 0.0f;

    switch (sector) {
        case 1: /* Sector 2 */
            t1 = (u_alpha * SQRT3 - u_beta) * 0.5f;
            t2 = (-u_alpha * SQRT3 - u_beta) * 0.5f;
            break;
        case 2: /* Sector 6 */
            t1 = (-u_alpha * SQRT3 + u_beta) * 0.5f;
            t2 = -u_beta;
            break;
        case 3: /* Sector 1 */
            t1 = u_beta;
            t2 = (u_alpha * SQRT3 - u_beta) * 0.5f;
            break;
        case 4: /* Sector 4 */
            t1 = -u_beta;
            t2 = (u_alpha * SQRT3 + u_beta) * 0.5f;
            break;
        case 5: /* Sector 3 */
            t1 = (u_alpha * SQRT3 + u_beta) * 0.5f;
            t2 = (-u_alpha * SQRT3 + u_beta) * 0.5f;
            break;
        case 6: /* Sector 5 */
            t1 = (-u_alpha * SQRT3 - u_beta) * 0.5f;
            t2 = u_alpha * SQRT3;
            break;
        default:
            t1 = 0.0f;
            t2 = 0.0f;
            sector = 1;
            break;
    }

    /* Over-modulation saturation check */
    if ((t1 + t2) > 1.0f) {
        float sum = t1 + t2;
        t1 /= sum;
        t2 /= sum;
    }
    float t0 = (1.0f - t1 - t2) * 0.5f;

    /* Symmetric center-aligned PWM duty cycles */
    float ta = 0.5f, tb = 0.5f, tc = 0.5f;
    switch (sector) {
        case 1: ta = t0 + t1 + t2; tb = t0 + t2;      tc = t0;           break;
        case 2: ta = t0 + t1;      tb = t0 + t1 + t2; tc = t0;           break;
        case 3: ta = t0;           tb = t0 + t1 + t2; tc = t0 + t2;      break;
        case 4: ta = t0;           tb = t0 + t1;      tc = t0 + t1 + t2; break;
        case 5: ta = t0 + t2;      tb = t0;           tc = t0 + t1 + t2; break;
        case 6: ta = t0 + t1 + t2; tb = t0;           tc = t0 + t1;      break;
    }

    pwm_out->t_a = ta;
    pwm_out->t_b = tb;
    pwm_out->t_c = tc;
    pwm_out->sector = sector;
}
