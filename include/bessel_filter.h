#ifndef BESSEL_FILTER_H
#define BESSEL_FILTER_H

#include <stdint.h>
#include <stddef.h>
#include "biphase_modulator.h"

/**
 * Bessel IIR filter (order 2, Fc=800kHz, Fs=2.5MHz)
 *
 * Equivalent to the analog Active Bessel Lowpass Filter
 * used in the dsPIC33 T.001 project.
 *
 * Characteristics:
 * - Order: 2 (2 poles)
 * - Cutoff frequency: 800 kHz
 * - Sample rate: 2.5 MHz
 * - Phase response: Near-linear (Bessel characteristic)
 * - Optimized for preserving Biphase-L signal shape
 */

// Filter state (Direct Form II)
typedef struct {
    // I channel state
    float x1_i, x2_i;  // Input history
    float y1_i, y2_i;  // Output history

    // Q channel state
    float x1_q, x2_q;  // Input history
    float y1_q, y2_q;  // Output history
} bessel_state_t;

/**
 * Initialize Bessel filter state
 */
void bessel_init(bessel_state_t *state);

/**
 * Process samples through Bessel filter
 * @param state Filter state
 * @param input Input I/Q samples
 * @param output Output filtered I/Q samples
 * @param num_samples Number of samples to process
 */
void bessel_process(bessel_state_t *state, const iq_sample_t *input,
                    iq_sample_t *output, size_t num_samples);

#endif // BESSEL_FILTER_H
