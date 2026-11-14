/**
 * @file LowPassFilter.cpp
 * @brief Implementation of the first-order IIR digital low-pass filter.
 */

#include "LowPassFilter.h"

/**
 * @brief Constructs a new LowPassFilter with a given smoothing coefficient.
 * 
 * @param alpha Weighting coefficient between 0.0 (maximum smoothing) and 1.0 (no filtering).
 */
LowPassFilter::LowPassFilter(float alpha) : _alpha(alpha), _state(0.0f) {}

/**
 * @brief Sets the initial internal state of the filter.
 * 
 * Prevents a filter ramp-up delay by preloading the first sensor reading.
 * 
 * @param state Initial baseline value.
 */
void LowPassFilter::init(float state) {
    _state = state;
}

/**
 * @brief Processes a new sample and returns the updated filtered state.
 * 
 * Filter properties:
 * - Cutoff frequency: fc = (Fs * alpha) / (2 * pi)
 * - Time constant:    tau = 1 / (Fs * alpha)
 * 
 * @param sample New input sample.
 * @return float Updated filtered state.
 */
float LowPassFilter::feed(float sample) {
    _state = (1.0f - _alpha) * _state + _alpha * sample;
    return _state;
}
