/**
 * @file RainTankController.ino
 * @brief Main firmware for the Industruino-based Rain Tank Controller.
 * 
 * Features:
 * - Hydrostatic 4-20mA water level sensing with digital low-pass filtering.
 * - Automatic refill solenoid valve control with low-level triggering and hysteresis shutoff.
 * - Anti-seizure periodic valve rinsing when inactive.
 * - Safety shutoff timer for continuous valve open duration.
 * - Local user interface on 128x64 LCD with 3-button keypad, menu carousel, and parameter editing.
 * - Remote Modbus TCP server (port 502) over Ethernet for Home Automation.
 * - Persistent configuration in SAMD21 internal flash memory.
 * - Hardware watchdog supervisor (WDTZero) for fail-safe embedded operation.
 */

// TODO: Add a mechanism to detect powered I/O board (vs usb only supply)
// TODO: Add tank shape options
// TODO: Use RTC ? Why for ? (need to insert battery)

#include <Arduino.h>
#include <ArduinoLog.h>
#include <Indio.h>
#include <FlashStorage.h>
#include <WDTZero.h>

#include "include/config.h"
#include "src/Button.h"
#include "src/Sensor_4_20mA.h"
#include "src/Relay.h"
#include "src/I2c.h"
#include "src/Network.h"
#include "src/ModbusService.h"

#include "src/LowPassFilter.h"
#include "src/Menu.h"
#include "src/Display.h"

// Hardware Watchdog Timer instance (SAMD21)
WDTZero watchdog;

// Non-volatile flash storage reservation for runtime configuration
FlashStorage(flashStorage, Configuration);

// -----------------------------------------------------------------------------
// Global Variables
// -----------------------------------------------------------------------------
unsigned long now;                 ///< Master system timestamp in milliseconds, refreshed each loop.
Configuration config;              ///< Active working system configuration.
MachineState state;                ///< Operational state (readings, active relays, alarms).
bool isClearAlarms;                ///< Deferred flag to clear active system alarms.
bool isSystemReset;                ///< Deferred flag to reboot via watchdog timeout.
bool isSaveConfiguration;          ///< Deferred flag to persist configuration to flash.
unsigned menuItemIdx;              ///< Index of the currently visible screen in menuItemList.

// -----------------------------------------------------------------------------
// System Subsystem Objects
// -----------------------------------------------------------------------------
I2c *i2c;                          ///< I2C bus helper.
Network *network;                  ///< W5500 Ethernet manager.
Display *display;                  ///< LCD display and UI driver.
Button *enterButton;               ///< Enter / Confirm button.
Button *upButton;                  ///< Up / Increment button.
Button *downButton;                ///< Down / Decrement button.
Sensor_4_20mA *waterLevelProbe;    ///< 4-20mA hydrostatic level probe.
Relay *relay_valve;                ///< Solenoid water fill valve relay.
Relay *relay_alarm;                ///< External visual/audible alarm output relay.
LowPassFilter *lowPassFilter;      ///< Lowpass filter for smoothing analog level measurements.

// Modbus TCP remote service
ModbusService *modbusService;      ///< Modbus TCP server handler.

// -----------------------------------------------------------------------------
// Menu System Definition
// -----------------------------------------------------------------------------
MenuItem *splashScreen;            ///< Boot splash screen (freed after boot delay).
MenuItem* menuItemList[] = {
  new TankLevelInfo("Tank level"),
  new OverviewMenuItem("Operating status"),
  new NetworkInfo("Network status", &network),
  new ParamIntMenuItem("Probe max. height", &config.probeMaxHeight_cm, 3, 0, 999, "cm"),
  new ParamIntMenuItem("Tank max. height", &config.tankMaxHeight_cm, 3, 0, 999, "cm"),
  new ParamIntMenuItem("Tank surface", &config.tankSurface_cm2, 5, 0, 65535, "cm2"), // Modbus register size is 16-bit / 65535 max
  new ParamIntMenuItem("Height Minimum", &config.waterLevelLow_cm, 3, 0, 999, "cm"),
  new ParamIntMenuItem("Height Hysteresis", &config.waterLevelHysteresis_cm, 2, 1, 99, "cm"),
  new ParamIntMenuItem("Valve-Max. time", &config.valveTimeout_min, 2, 1, 59, "Min."),
  new ParamIntMenuItem("Rinse period", &config.valveRinsePeriod_day, 2, 1, 31, "Day"),
  new ParamIntMenuItem("Rinse time", &config.valveRinseDuration_min, 2, 1, 59, "Min."),
  new ParamBoolMenuItem("DHCP enabled", &config.isDhcp),
  new ParamIPMenuItem("Static IP address", &config.ipAddress, &config.isDhcp),
  new ParamIPMenuItem("Static gateway", &config.ipGateway, &config.isDhcp),
  new ParamIPMenuItem("Static subnet", &config.ipSubnet, &config.isDhcp),
  new ParamIPMenuItem("Static DNS", &config.ipDNS, &config.isDhcp),
  new ConfirmMenuItem("Reboot system", &isSystemReset),
  new ConfirmMenuItem("Save configuration", &isSaveConfiguration),
  new ConfirmMenuItem("Clear alarms", &isClearAlarms)
};

/**
 * @brief Saves the current configuration to SAMD21 flash memory.
 * 
 * Stamps the structure with CONFIGURATION_MAGIC_NUMBER to ensure data integrity
 * on future boots.
 * 
 * @param config Pointer to configuration struct to persist.
 */
void saveConfiguration(Configuration *config) {
  config->magicNumber = CONFIGURATION_MAGIC_NUMBER;
  flashStorage.write(*config);
}

/**
 * @brief Loads configuration from internal flash or writes factory defaults.
 * 
 * Validates the stored structure against CONFIGURATION_MAGIC_NUMBER.
 * If uninitialized or invalid, initializes factory defaults and commits them.
 * 
 * @return Configuration The loaded or default system configuration.
 */
Configuration loadConfiguration() {
  Configuration config = flashStorage.read();

  // Validate magic number; load defaults if corrupted or uninitialized
  // FIXME: Add a configuration version, a size, a crc, validate all fields even when magic number matches
  if (config.magicNumber != CONFIGURATION_MAGIC_NUMBER) {
    config.probeMaxHeight_cm = 300;              // 20mA corresponds to 3m hydrostatic probe height
    config.tankMaxHeight_cm = 125;               // Overflow level height
    config.tankSurface_cm2 = 33650;              // Diameter 223cm minus 8cm walls
    config.waterLevelLow_cm = 24;                // ~800L threshold (pump minimum submerged level = 15cm)
    config.waterLevelHysteresis_cm = 6;          // ~200L hysteresis band
    config.valveRinseDuration_min = 1;           // 10L assuming 10L/min flow rate
    config.valveRinsePeriod_day = 30;            // 30 days maximum idle period before anti-seizure rinse
    config.valveTimeout_min = 30;                // 300L safety cutoff assuming 10L/min
    config.isDhcp = true;                        // Default to DHCP
    config.ipAddress = IPAddress(192, 168, 1, 234); // Fallback static IP
    config.ipSubnet = IPAddress(255, 255, 255, 0);  // Static subnet
    config.ipGateway = IPAddress(192, 168, 1, 1);   // Static gateway
    config.ipDNS = IPAddress(192, 168, 1, 1);       // Static DNS

    saveConfiguration(&config);
  }
  return config;
}

/**
 * @brief Resets UI view back to default primary screen (Tank Level Gauge).
 */
void MenuDefault() {
  if (menuItemIdx != 0) {
    menuItemIdx = 0;
    display->setRenderer(menuItemList[menuItemIdx]);
  }
}

/**
 * @brief Arduino initialization routine.
 * 
 * Configures hardware peripherals, communication buses, inputs/outputs,
 * network stack, Modbus service, and arms the hardware watchdog.
 */
void setup() {
  now = millis();

  // Initialize USB CDC serial for logging
  SerialUSB.begin(115200);
  //while (millis() < 2000); // Allow 2 seconds for USB serial attachment/initial logs

  Log.begin(LOG_LEVEL_INFO, &SerialUSB);
  Log.info("Industruino RainTank controller starting...\n");

  // Load configuration from internal flash
  config = loadConfiguration();

  // Setup buttons with debouncing (50ms) and hold thresholds (1000ms / 500ms)
  enterButton = new Button(PIN_BTN_ENTER, LOW);
  enterButton->begin(&now, Button::Mode::LongPress, DEBOUNCE_DELAY, LONG_PRESS_TIME); 

  upButton = new Button(PIN_BTN_UP, LOW);
  upButton->begin(&now, Button::Mode::Repeat, DEBOUNCE_DELAY, LONG_PRESS_TIME, REPEAT_DELAY); 

  downButton = new Button(PIN_BTN_DOWN, LOW);
  downButton->begin(&now, Button::Mode::Repeat, DEBOUNCE_DELAY, LONG_PRESS_TIME, REPEAT_DELAY); 

  // Setup 4-20mA sensor on Industruino 14-bit ADC channel 1
  Indio.setADCResolution(14);
  waterLevelProbe = new Sensor_4_20mA(CH_LEVEL_PROBE);
  waterLevelProbe->begin(PROBE_SAMPLING_PERIOD);
  state.waterLevel_mA = max(4.0, min(20.0, waterLevelProbe->read()));
  state.waterLevel_cm = config.probeMaxHeight_cm * (state.waterLevel_mA - 4.0) / (20.0 - 4.0);

  // Setup relay outputs (solenoid valve with safety timeout, alarm without timeout)
  relay_valve = new Relay(CH_VALVE, "valve");
  state.valve = relay_valve;
  relay_valve->begin(&now, config.valveTimeout_min, false);
  relay_alarm = new Relay(CH_ALARM, "alarm");
  relay_alarm->begin(&now, 0, false);
  state.alarm = relay_alarm;

  // Setup digital low-pass filter with first sensor reading
  lowPassFilter = new LowPassFilter(LOWPASS_COEFFICIENT);
  lowPassFilter->init(state.waterLevel_mA);

  // Setup 128x64 graphic LCD display & initial splash screen
  display = new Display(PIN_BACKLIGHT, LCD_REFRESH_PERIOD);
  display->begin(&now);
  splashScreen = new SplashScreen();
  display->setRenderer(splashScreen);
  display->update(now, nullptr, nullptr, true); // Force initial refresh to show splash screen immediately

  // Setup network and Modbus TCP service
  i2c = new I2c();
  network = new Network();
  network->init(i2c);

  if (network->isAvailable()) {
    if (config.isDhcp) {
      display->setLine(7, "DHCP ongoing (60s) ...");
      display->update(now, nullptr, nullptr, true); // Force refresh to display DHCP progress status
      network->begin(INADDR_NONE);
    } else {
      network->begin(config.ipAddress, config.ipDNS, config.ipGateway, config.ipSubnet);
    }
    modbusService = new ModbusService();
    modbusService->begin(network);
  }

  // Reset operational machine states
  state.isRinsing = false;
  state.alarmCode = 0; // Clear all alarms
  state.probeErrorCnt = 0; // Clear leaky bucket

  menuItemIdx = 0; // Default to primary menu item
  isClearAlarms = false;

  // Arm SAMD21 hardware watchdog timer with 2-second timeout
  watchdog.setup(WDT_HARDCYCLE2S);
}

/**
 * @brief Main execution loop.
 * 
 * Orchestrates:
 * 1. Watchdog refresh.
 * 2. Deferred state actions (save flash, clear alarms, system reset).
 * 3. Modbus TCP client polling and telemetry exchange.
 * 4. Button polling, display backlight wake-up, and menu navigation.
 * 5. Water level sampling, digital filtering, and refill hysteresis regulation.
 * 6. Periodic valve rinsing.
 * 7. Committing relay hardware outputs.
 * 8. Display refresh.
 */
void loop() {
  now = millis();

  // --- Watchdog & System Reset ---
  if (!isSystemReset) {
    watchdog.clear(); // Refresh watchdog; if isSystemReset is true, letting watchdog expire forces a hardware reboot
  }

  // --- Deferred Commands ---
  // Save configuration to flash
  if (isSaveConfiguration) {
    isSaveConfiguration = false;
    saveConfiguration(&config);
    Log.info("Configuration saved\n");
    MenuDefault();
  }

  // Clear alarm flags
  if (isClearAlarms) {
    isClearAlarms = false;
    state.alarmCode = 0;
    state.probeErrorCnt = 0;
    Log.info("Alarms cleared\n");
    MenuDefault();
  }

  // --- Modbus Service ---
  if (network->isAvailable()) {
    // If the initial DHCP attempt fails, e.g. if no cable was attached at startup, a retry could be initiated here but,
    // the `begin()` function in the Ethernet2 library is blocking for 60 seconds (no ways to check the link status either).
    network->update();
    modbusService->update(config, state, isSaveConfiguration, isClearAlarms, isSystemReset);
  }

  // --- Button & Keypad Processing ---
  Button::Event enterButtonEvent = enterButton->update();
  Button::Event upButtonEvent = upButton->update();
  Button::Event downButtonEvent = downButton->update();
  bool buttonEvent = 
    (enterButtonEvent != Button::Event::None) ||
    (upButtonEvent != Button::Event::None) ||
    (downButtonEvent != Button::Event::None);

  // Restore LCD backlight on any button press
  display->wake(now, buttonEvent);

  // --- Menu Navigation & Timeout ---
  static unsigned long lastEvent = 0;
  if (buttonEvent) {
    lastEvent = now;

    // Pass button events to active menu item; if not consumed, navigate carousel
    bool eventCaptured = menuItemList[menuItemIdx]->update(enterButtonEvent, upButtonEvent, downButtonEvent);
    if (!eventCaptured) {
      int nMenu = sizeof(menuItemList) / sizeof(menuItemList[0]);
      if (upButtonEvent == Button::Event::ShortPressed) {
        do {
          menuItemIdx = (menuItemIdx >= (nMenu - 1)) ? 0 : menuItemIdx + 1;
        } while (!menuItemList[menuItemIdx]->isVisible());
        display->setRenderer(menuItemList[menuItemIdx]);
      } else if (downButtonEvent == Button::Event::ShortPressed) {
        do {
          menuItemIdx = (menuItemIdx == 0) ? nMenu - 1 : menuItemIdx - 1;
        } while (!menuItemList[menuItemIdx]->isVisible());      
        display->setRenderer(menuItemList[menuItemIdx]);
      }
    }
  } else {
    // Return to default screen after period of inactivity
    if ((now - lastEvent) > DISPLAY_DEFAULT_DELAY) {
      MenuDefault();
    }
  }

  // --- Valve Safety Timeout Monitoring ---
  if (relay_valve->update() == Relay::Event::Timeout) {
    Log.warning("Valve opened for too long\n");
    state.isRinsing = false; // Cancel rinse
    state.alarmCode |= ALARM_VALVE;
  }
  relay_alarm->update(); // Alarm relay has no automatic timeout

  // --- Water Level Probe Sampling & Tank Regulation ---
  Sensor_4_20mA::Event samplingEvent = waterLevelProbe->update(now);

  int isValveOn = relay_valve->isActive();
  switch (samplingEvent) {
    case Sensor_4_20mA::Event::NewSample:
      // Bucket leak, decrease by one drop
      if (state.probeErrorCnt > 0) state.probeErrorCnt--;

      state.waterLevel_mA = waterLevelProbe->read();
      // Linear conversion from 4-20mA to water height (cm) with low-pass filtering
      state.waterLevel_cm = config.probeMaxHeight_cm * (lowPassFilter->feed(state.waterLevel_mA) - 4.0) / (20.0 - 4.0);
      
      // Hysteresis refill control logic
      if (state.waterLevel_cm < config.waterLevelLow_cm) {
        // Water below minimum threshold: open valve if no alarm is set
        if (!relay_valve->isActive() && state.alarmCode == 0) {
          Log.info("Water level below minimum\n");
          isValveOn = true;
        }
      } else if (state.waterLevel_cm > (config.waterLevelLow_cm + config.waterLevelHysteresis_cm)) {
        // Water above refill threshold + hysteresis: close valve
        if (relay_valve->isActive()) Log.info("Water level above minimum + hysteresis\n");
        isValveOn = false;
      }
      break;

    case Sensor_4_20mA::Event::OutOfRange:
      // Electrical fault detected on sensor loop
      if (state.probeErrorCnt == 0) {
        Log.warning("Invalid probe reading (reported only once)\n");
      } 
      if (state.probeErrorCnt < (UINT16_MAX - ALARM_PROBE_COST)) {
        state.probeErrorCnt += ALARM_PROBE_COST;
      } else {
        state.probeErrorCnt = UINT16_MAX;
      }
      break;

    default:
      // No sampling event to process
      break;
  }

  // --- Periodic Anti-Seizure Valve Rinsing ---
  // Opens the valve for a brief period if left idle for valveRinsePeriod_day days
  static unsigned long rinsingStartTime = 0; 
  if (state.isRinsing) {
    if ((now - rinsingStartTime) > (config.valveRinseDuration_min * 60UL * 1000UL)) {
      state.isRinsing = false;
    }
  } else {
    // Check if idle duration since last valve state change exceeds configured period
    if ((now - relay_valve->getLastChangeTime()) > (config.valveRinsePeriod_day * 24UL * 60UL * 60UL * 1000UL)) {
      rinsingStartTime = now;
      state.isRinsing = true;
    }
  }

  // If the probe leaky bucket overflow, declare a permanent error
  if (state.probeErrorCnt > ALARM_PROBE_THRESHOLD) {
    isValveOn = false;
    state.alarmCode |= ALARM_PROBE;
    state.waterLevel_mA = 0.0;
    state.waterLevel_cm = 0.0;
  }

  // --- Commit Physical Relay States ---
  // Valve is energized if refill is required or rinsing is active
  (isValveOn || state.isRinsing) ? relay_valve->activate() : relay_valve->deactivate();
  // Alarm relay is energized if any alarm code is non-zero
  (state.alarmCode == 0) ? relay_alarm->deactivate() : relay_alarm->activate(); 

  // --- Display Splash Screen Timing ---
  if (splashScreen != nullptr) {
    if (now > SPLASH_SCREEN_DELAY) {
      delete splashScreen;
      splashScreen = nullptr;
      display->setRenderer(menuItemList[menuItemIdx]);
    }
    return; // Skip drawing main UI while splash screen is showing
  }

  // --- Display Refresh ---
  display->update(now, &config, &state); 
}
