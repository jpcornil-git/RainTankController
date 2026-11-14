/**
 * @file Relay.cpp
 * @brief Implementation of the Relay output controller with auto-shutoff timeout.
 */

#include <ArduinoLog.h>
#include <Indio.h>
#include "Relay.h"

/**
 * @brief Constructs a new Relay object.
 * 
 * @param channel Industruino digital output channel number (1 to 8).
 * @param name Descriptive label for log messages.
 */
Relay::Relay(int channel, const char* name) : _channel(channel), _name(name) {
}

/**
 * @brief Configures channel hardware mode, sets timeout, and establishes initial state.
 * 
 * @param now Pointer to the system timestamp variable.
 * @param timeoutMin Timeout in minutes before auto-shutoff (0 disables auto-shutoff).
 * @param isActivated True to activate immediately, false to deactivate.
 */
void Relay::begin(unsigned long *now, unsigned long timeoutMin, bool isActivated) {
    _timeout = timeoutMin * 60 * 1000;
    _isActive = !isActivated; // Force state change update on initial call
    _pNow = now;
    isActivated ? activate() : deactivate();
    Indio.digitalMode(_channel, OUTPUT);
}

/**
 * @brief Returns the relay's descriptive name.
 * 
 * @return const char* Relay name string.
 */
const char* Relay::getName() {
    return _name;
} 

/**
 * @brief Returns the current activation state.
 * 
 * @return bool True if active/energized, false if inactive.
 */
bool Relay::isActive() {
    return _isActive;
}

/**
 * @brief Energizes the relay output.
 * 
 * Performs the hardware I2C write only if the relay is not already active,
 * reducing bus load, records activation timestamp, and logs the event.
 */
void Relay::activate() {
    // Only update if state effectively changed to limit i2c activity
    if (!_isActive) { 
        _isActive = true;
        Indio.digitalWrite(_channel, HIGH);
        _timerStartTime = *_pNow;
        Log.info("%s switched on\n", _name);
    }
}

/**
 * @brief De-energizes the relay output.
 * 
 * Performs hardware I2C write only if the relay is currently active,
 * updates the timer baseline, and logs the event.
 */
void Relay::deactivate() {
    // Only update if state effectively changed to limit i2c activity
    if (_isActive) { 
        _isActive = false;
        Indio.digitalWrite(_channel, LOW);
        _timerStartTime = *_pNow;
        Log.info("%s switched off\n", _name);
    }
}

/**
 * @brief Returns the timestamp of the last state change.
 * 
 * @return unsigned long Timestamp in ms when relay switched state.
 */
unsigned long Relay::getLastChangeTime() {
    return _timerStartTime;
}

/**
 * @brief Checks whether the relay has exceeded its timeout.
 * 
 * If active and runtime exceeds _timeout, deactivates the output and
 * returns Event::Timeout to trigger alarms or state transitions.
 * 
 * @return Relay::Event Event::Timeout if auto-shutoff triggered, otherwise Event::None.
 */
Relay::Event Relay::update() {
    if (_isActive) {
        if (_timeout != 0 && (*_pNow - _timerStartTime) > _timeout) {
            deactivate();
            return Relay::Event::Timeout;
        }
    }
    return Relay::Event::None;
}
