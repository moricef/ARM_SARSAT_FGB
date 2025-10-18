#include "bessel_filter.h"
#include <string.h>

// Bessel IIR filter coefficients (order 2, Fc=800kHz, Fs=2.5MHz)
// Equivalent to analog Bessel Active Filter used in dsPIC33 project
// Direct Form II implementation: y[n] = b0*x[n] + b1*x[n-1] + b2*x[n-2] - a1*y[n-1] - a2*y[n-2]

static const float b0 = 0.2693698845f;
static const float b1 = 0.5387397691f;
static const float b2 = 0.2693698845f;
static const float a1 = 0.0056757937f;
static const float a2 = 0.0718037444f;

void bessel_init(bessel_state_t *state) {
    // Clear state variables (Direct Form II)
    state->x1_i = 0.0f;
    state->x2_i = 0.0f;
    state->y1_i = 0.0f;
    state->y2_i = 0.0f;

    state->x1_q = 0.0f;
    state->x2_q = 0.0f;
    state->y1_q = 0.0f;
    state->y2_q = 0.0f;
}

void bessel_process(bessel_state_t *state, const iq_sample_t *input,
                    iq_sample_t *output, size_t num_samples) {

    for (size_t n = 0; n < num_samples; n++) {
        float x_i = (float)input[n].i;
        float x_q = (float)input[n].q;

        // IIR filter for I channel (Direct Form II)
        float y_i = b0*x_i + b1*state->x1_i + b2*state->x2_i
                    - a1*state->y1_i - a2*state->y2_i;

        // IIR filter for Q channel
        float y_q = b0*x_q + b1*state->x1_q + b2*state->x2_q
                    - a1*state->y1_q - a2*state->y2_q;

        // Update state variables (shift register)
        state->x2_i = state->x1_i;
        state->x1_i = x_i;
        state->y2_i = state->y1_i;
        state->y1_i = y_i;

        state->x2_q = state->x1_q;
        state->x1_q = x_q;
        state->y2_q = state->y1_q;
        state->y1_q = y_q;

        // Output filtered sample
        output[n].i = (int16_t)y_i;
        output[n].q = (int16_t)y_q;
    }
}
