#ifndef RELAY
#define RELAY

/**
 * @file Relay.h
 * @brief Output relay controller with safety timeout for Industruino IND.I/O.
 * 
 * Manages digital output channels (valves, alarms, etc.) with state-change
 * filtering to minimize I2C bus traffic, and an optional auto-deactivation timeout.
 */

class Relay {
public:
    /**
     * @brief Relay status events emitted during update() cycles.
     */
    enum Event {
        None          = 0,     ///< No event occurred.
        Timeout       = 1 << 0 ///< Relay exceeded its maximum permitted on-time and was auto-deactivated.
    };

    /**
     * @brief Construct a new Relay instance.
     * 
     * @param channel Industruino digital output channel number (1 to 8).
     * @param name Descriptive identifier used for logging (e.g. "valve", "alarm").
     */
    Relay(int channel, const char *name);

    /**
     * @brief Default destructor.
     */
    ~Relay() = default;

    /**
     * @brief Initialize output pin mode, timeout settings, and initial state.
     * 
     * @param now Pointer to the shared system millisecond timestamp.
     * @param timeoutMin Safety timeout duration in minutes (0 disables timeout).
     * @param isActivated Initial desired state (true = energize relay, false = de-energize).
     */
    void begin(unsigned long *now, unsigned long timeoutMin = 0, bool isActivated = false);

    /**
     * @brief Energize the relay output (set channel HIGH).
     * 
     * Updates hardware only if currently inactive to minimize I2C transactions.
     * Starts the timeout timer.
     */
    void activate();

    /**
     * @brief De-energize the relay output (set channel LOW).
     * 
     * Updates hardware only if currently active. Resets the change timestamp.
     */
    void deactivate();

    /**
     * @brief Monitor relay on-time against configured safety timeout.
     * 
     * If active and timeout is reached, automatically deactivates the relay and
     * returns Event::Timeout.
     * 
     * @return Event Event::Timeout if expired, otherwise Event::None.
     */
    Event update();

    /**
     * @brief Check whether the relay is currently energized.
     * 
     * @return bool True if active (HIGH), false if inactive (LOW).
     */
    bool isActive();

    /**
     * @brief Get the system timestamp of the last state transition.
     * 
     * @return unsigned long Timestamp in milliseconds when the state last changed.
     */
    unsigned long getLastChangeTime();

    /**
     * @brief Get the descriptive name of the relay.
     * 
     * @return const char* Pointer to the relay name string.
     */
    const char* getName();

private:
    int _channel;                  ///< Industruino digital output channel number.
    const char *_name;             ///< Relay identifier name for logging.
    unsigned long *_pNow;          ///< Pointer to shared system time in milliseconds.
    unsigned long _timeout;        ///< Maximum on-time duration in milliseconds (0 = disabled).

    unsigned long _timerStartTime; ///< Timestamp (ms) when relay was energized or state changed.
    bool _isActive;                ///< Current relay state (true = energized / HIGH).
};

#endif // RELAY
