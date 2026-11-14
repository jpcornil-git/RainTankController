/**
 * @file Sensor_4_20mA.cpp
 * @brief Implementation of the 4-20mA current loop sensor reader.
 */

#include <Arduino.h>
#include <ArduinoLog.h>
#include <Wire.h>
#include <Indio.h>
#include "Sensor_4_20mA.h"
#include "../include/config.h"

// 4-20mA Limits to declare an electrical fault
const float MIN_CURRENT_VALUE = 3.8;  ///< Current below 3.8mA indicates an open circuit / broken cable.
const float MAX_CURRENT_VALUE = 21.0; ///< Current above 21.0mA indicates an electrical short / overcurrent fault.

// MCP3422 4ch A/D converter I2C address on Industruino baseboard
const int MCP3422_ADDRESS = 0x68;

/**
 * @brief Constructs a new Sensor_4_20mA object for a given channel.
 * 
 * @param channel Industruino analog channel (1 to 4).
 */
Sensor_4_20mA::Sensor_4_20mA(int channel) : _channel(channel) {
}

/**
 * @brief Configures channel hardware mode and performs initial conversion.
 * 
 * Sets the Industruino analog front-end to current mode (mA) and samples
 * the initial reading so read() returns a valid value immediately.
 * 
 * @param samplingPeriod Sampling period in milliseconds.
 */
void Sensor_4_20mA::begin(unsigned long samplingPeriod) {
    _samplingPeriod = samplingPeriod;
    _lastSensorReading = 0;
    Indio.analogReadMode(_channel, mA);
    _last_sample = Indio.analogRead(_channel);
}

/**
 * @brief Periodically polls the analog channel and checks electrical validity.
 * 
 * When the sampling period has elapsed:
 * 1. Reads the current via Indio.analogRead(_channel).
 * 2. Compares against MIN_CURRENT_VALUE (3.8mA) and MAX_CURRENT_VALUE (21.0mA).
 * 3. Returns Event::OutOfRange on error or Event::NewSample on success.
 * 
 * @param now Current timestamp in milliseconds.
 * @return Sensor_4_20mA::Event The resulting sensor event.
 */
Sensor_4_20mA::Event Sensor_4_20mA::update(unsigned long now) {
    if ((now - _lastSensorReading) > _samplingPeriod) {
        _lastSensorReading = now;
        _last_sample = Indio.analogRead(_channel);

        // Check if current loop signal is outside valid boundaries
        if ((_last_sample < MIN_CURRENT_VALUE) || (_last_sample > MAX_CURRENT_VALUE)) {
            return Sensor_4_20mA::Event::OutOfRange;
        }
        return Sensor_4_20mA::Event::NewSample;
    }
    return Sensor_4_20mA::Event::None;
}

/**
 * @brief Returns the most recently acquired current sample.
 * 
 * @return float Measured current in mA.
 */
float Sensor_4_20mA::read() {
    return _last_sample;
}
