#ifndef LOWPASSFILTER_H
#define LOWPASSFILTER_H

/**
 * @file LowPassFilter.h
 * @brief First-order exponential moving average (EMA) low-pass filter.
 * 
 * Used to smooth analog current sensor readings and filter out high-frequency
 * noise and water surface ripples.
 */

class LowPassFilter {
public:
    /**
     * @brief Construct a new LowPassFilter instance.
     * 
     * @param alpha Smoothing factor (0.0 < alpha <= 1.0). Smaller values provide heavier smoothing.
     */
    LowPassFilter(float alpha);

    /**
     * @brief Default destructor.
     */
    ~LowPassFilter() = default;

    /**
     * @brief Initialize or reset the filter's internal state.
     * 
     * @param state Initial value (usually the first raw sensor sample).
     */
    void init(float state);

    /**
     * @brief Feed a new raw sample into the filter and compute the updated state.
     * 
     * Calculation: state = (1.0 - alpha) * state + alpha * sample
     * 
     * @param sample New input sample.
     * @return float The newly computed filtered output.
     */
    float feed(float sample);

private:
    float _alpha; ///< Filter weighting coefficient (0.0 to 1.0).
    float _state; ///< Current filtered state output.
};

#endif // LOWPASSFILTER_H
