#ifndef CONFIG_H
#define CONFIG_H

/**
 * @file config.h
 * @brief Global hardware pinout, timing definitions, and data structures.
 * 
 * Defines hardware channel mappings for the Industruino IND.I/O platform,
 * operating thresholds, LCD timing constants, flash storage structure,
 * and the machine state container.
 */

#include "Ethernet2.h"
#include "../src/Relay.h"

/**
 * @brief Standard function status return code.
 */
enum ErrorCode { 
    SUCCESS, ///< Operation completed without error.
    ERROR    ///< Operation failed.
};

// -----------------------------------------------------------------------------
// Hardware Pinout Definitions
// -----------------------------------------------------------------------------

/// PWM pin for controlling the LCD backlight brightness.
const int PIN_BACKLIGHT = 26;

// Keypad input pins
const int PIN_BTN_DOWN  = 23; ///< Down navigation button pin.
const int PIN_BTN_ENTER = 24; ///< Enter / confirm button pin.
const int PIN_BTN_UP    = 25; ///< Up navigation button pin.

// -----------------------------------------------------------------------------
// Industruino IND.I/O Channel Mappings
// -----------------------------------------------------------------------------

/// Analog input channel (1 to 4) connected to the 4-20mA hydrostatic level sensor.
const int CH_LEVEL_PROBE = 1;

// Digital output channels (1 to 8)
const int CH_VALVE  = 1; ///< Digital output channel driving the water fill solenoid valve.
const int CH_ALARM  = 2; ///< Digital output channel driving the alarm indicator / siren.

// -----------------------------------------------------------------------------
// System Alarm Bitmasks
// -----------------------------------------------------------------------------
const int ALARM_PROBE = 1 << 0; ///< Bit 0: Hydrostatic level sensor reading out of valid 4-20mA bounds.
const int ALARM_VALVE = 1 << 1; ///< Bit 1: Refill valve remained open continuously beyond safety timeout.

// -----------------------------------------------------------------------------
// UI & Display Timing (Milliseconds)
// -----------------------------------------------------------------------------
const int DISPLAY_DEFAULT_DELAY = 5000;         ///< Inactivity duration before reverting to the default screen (ms).
const unsigned long SPLASH_SCREEN_DELAY = 5000; ///< Display duration of the startup splash screen (ms).
const int LCD_REFRESH_PERIOD = 500;             ///< Display refresh rate limiting interval (ms).
const int LCD_BACKLIGHT_MEDIUM_DELAY = 3000;    ///< Idle duration before dimming backlight to 50% (ms).
const int LCD_BACKLIGHT_OFF_DELAY = LCD_BACKLIGHT_MEDIUM_DELAY + 5000; ///< Idle duration before turning backlight off (ms).

// -----------------------------------------------------------------------------
// Button Debounce and Hold Thresholds (Milliseconds)
// -----------------------------------------------------------------------------
const unsigned long DEBOUNCE_DELAY  = 50;   ///< Debounce stabilization delay (ms).
const unsigned long LONG_PRESS_TIME = 1000; ///< Minimum hold time to trigger a long press event (ms).
const unsigned long REPEAT_DELAY    = 500;  ///< Repeat interval while button is continuously held (ms).

// -----------------------------------------------------------------------------
// Sensor & Filter Configuration
// -----------------------------------------------------------------------------
const int PROBE_SAMPLING_PERIOD = 1000;  ///< 4-20mA sensor sampling period in milliseconds.
const float LOWPASS_COEFFICIENT = 0.01;  ///< Lowpass filter smoothing alpha (0.01 corresponds to ~100s time constant at 1 Hz).

// -----------------------------------------------------------------------------
// Persistent Configuration (Flash Storage)
// -----------------------------------------------------------------------------
const int CONFIGURATION_MAGIC_NUMBER = 0x0745; ///< Magic number used to validate valid flash memory records.

/**
 * @brief Persistent configuration settings stored in internal non-volatile flash.
 * FIXME: Use fixed-size records, e.g. replace bool and IPAddress by uint8_t
 */
struct Configuration {
    uint16_t magicNumber;               ///< Validation identifier (CONFIGURATION_MAGIC_NUMBER).
    unsigned probeMaxHeight_cm;         ///< Maximum water height (cm) corresponding to 20mA probe current.
    unsigned tankMaxHeight_cm;          ///< Total tank height up to the overflow lip (cm).
    unsigned tankSurface_cm2;           ///< Tank cross-sectional area (cm2), used for volume estimation (assumes vertical cylindrical tank).
    unsigned waterLevelLow_cm;          ///< Low-water refill trigger threshold (cm).
    unsigned waterLevelHysteresis_cm;   ///< Refill shutoff hysteresis offset above waterLevelLow_cm (cm).
    unsigned valveTimeout_min;          ///< Maximum continuous open time before emergency shutoff and alarm (minutes).
    unsigned valveRinsePeriod_day;      ///< Maximum idle time before triggering an anti-seizure valve rinse cycle (days).
    unsigned valveRinseDuration_min;    ///< Duration of the periodic anti-seizure rinse cycle (minutes).
    bool isDhcp;                        ///< True to use DHCP for network setup; false for static IP.
    IPAddress ipAddress;                ///< Static IPv4 address.
    IPAddress ipGateway;                ///< Static default gateway IP.
    IPAddress ipSubnet;                 ///< Static subnet mask.
    IPAddress ipDNS;                    ///< Static primary DNS server IP.
};

// -----------------------------------------------------------------------------
// Machine Runtime State
// -----------------------------------------------------------------------------

/**
 * @brief Operational runtime state container shared across firmware modules.
 */
struct MachineState {
    float waterLevel_mA;    ///< Instantaneous raw current reading reported by the probe (mA).
    float waterLevel_cm;    ///< Filtered and calibrated water level height (cm).
    Relay *valve;           ///< Pointer to solenoid valve output relay controller.
    Relay *alarm;           ///< Pointer to alarm annunciator output relay controller.
    bool isRinsing;         ///< True while an automated anti-seizure valve rinse cycle is active.
    unsigned alarmCode;     ///< Active alarm bitfield combining ALARM_PROBE and ALARM_VALVE flags.
};

#endif // CONFIG_H
