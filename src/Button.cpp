/**
 * @file Button.cpp
 * @brief Implementation of the debounced Button input controller.
 */

#include <Arduino.h>
#include "Button.h"

/**
 * @brief Constructs a new Button object and sets up the hardware pin.
 * 
 * @param pin The digital pin number connected to the physical button.
 * @param pressedState Electrical state corresponding to pressed (LOW or HIGH).
 */
Button::Button(int pin, int pressedState) : _pin(pin), _pressedState(pressedState) {
    pinMode(_pin, INPUT);
}

/**
 * @brief Reads the instantaneous electrical level from the GPIO pin.
 * 
 * Translates the hardware voltage reading into logical State::Pressed or State::Released
 * based on the configured _pressedState.
 * 
 * @return Button::State Raw un-debounced state.
 */
Button::State Button::_readPin() {
    return digitalRead(_pin) == _pressedState ? State::Pressed : State::Released;
}

/**
 * @brief Initializes timing parameters, operating mode, and baseline state.
 * 
 * @param now Pointer to the shared millisecond clock variable.
 * @param mode Mode::LongPress (single hold event) or Mode::Repeat (repeating events).
 * @param debounceDelay Time in ms required to filter mechanical contact bounce.
 * @param longPressDelay Time in ms required to activate long press or first repeat.
 * @param repeatInterval Time in ms between repeat events while holding.
 */
void Button::begin(unsigned long *now, Mode mode, int debounceDelay, long int longPressDelay, long int repeatInterval) {
    _pNow = now;
    _mode = mode;
    _debounceDelay = debounceDelay;
    _longPressDelay = longPressDelay;
    _repeatInterval = repeatInterval;

    // Initialize state with the current raw reading
    _state = _readPin();
    _lastRawState = _state;
    _lastDebounceTime = 0;
    _pressStartTime = 0;
    _longPressFired = false;
}

/**
 * @brief Returns the current debounced physical state of the button.
 * 
 * @return Button::State Current debounced state (Pressed or Released).
 */
Button::State Button::getState() {
    return _state;
}

/**
 * @brief Updates debouncing timers, tracks transitions, and generates button events.
 * 
 * Steps performed:
 * 1. Reads raw pin input and detects transitions to reset the debounce timer.
 * 2. Confirms state changes once the input remains stable beyond _debounceDelay.
 * 3. Dispatches ShortPressed on release if the button was released prior to long press delay.
 * 4. Checks hold durations while pressed to fire LongPressed or periodic Repeat events.
 * 
 * @return Button::Event Event detected during this update cycle, or Event::None.
 */
Button::Event Button::update() {
    State rawState = _readPin();

    // --- Debounce Logic ---
    // If the raw pin state changed since the last check, restart the debounce timer
    if (rawState != _lastRawState) {
        _lastRawState = rawState;
        _lastDebounceTime = *_pNow;
    }

    // Check if the signal has remained stable for longer than the debounce threshold
    if ((*_pNow - _lastDebounceTime) > _debounceDelay) {
        // State has stabilized to a new value
        if (rawState != _state) {
            _state = rawState;

            if (_state == State::Pressed) {
                // Button was physically pressed down
                _pressStartTime = *_pNow;
                _longPressFired = false;
            } else { // State::Released
                // Button was released: if released before long press triggered, declare a short press
                if (!_longPressFired) {
                    return Event::ShortPressed;
                }
            }
        }
    }

    // --- Hold / Long Press / Repeat Logic ---
    // Check for holding conditions while the button is actively pressed down
    if (_state == State::Pressed) {
        if (_mode == Mode::LongPress) {
            // Mode::LongPress: trigger a single event after _longPressDelay
            if (!_longPressFired && ((*_pNow - _pressStartTime) >= _longPressDelay)) {
                _longPressFired = true;
                return Event::LongPressed;
            }
        } else { // Mode::Repeat
            // Mode::Repeat: use _longPressDelay for first repeat, then _repeatInterval for subsequent repeats
            unsigned long currentIInterval = _longPressFired ? _repeatInterval : _longPressDelay;

            if ((*_pNow - _pressStartTime) >= currentIInterval) {
                _longPressFired = true;
                _pressStartTime = *_pNow; // Reset baseline for the next repeat interval
                return Event::Repeat;
            }
        }
    }

    return None;
}
