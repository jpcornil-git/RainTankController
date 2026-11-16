/**
 * @file ModbusService.cpp
 * @brief Implementation of the Modbus TCP server for telemetry and remote control.
 */

#include <ArduinoLog.h>
#include "../include/config.h"
#include "ModbusService.h"

/**
 * @brief Constructs a new ModbusService instance.
 */
ModbusService::ModbusService() {
}

/**
 * @brief Initializes Ethernet listener and configures Modbus register maps.
 * 
 * Register allocation:
 * - 4 Coils (save config, clear alarms, system reset, DHCP enable)
 * - 6 Discrete Inputs (valve status, alarm status, rinsing, DHCP, probe fault, valve fault)
 * - 16 Holding Registers (system parameters & network settings)
 * - 18 Input Registers (system parameters, network settings, water level uA, water level mm)
 * 
 * @param network Pointer to Network object to verify connectivity.
 * @return bool True if listener and server were initialized, false on error.
 */
bool ModbusService::begin(Network *network) {
    bool success = false;
    if (network->isAvailable()) {
        _modbusServer = new EthernetServer(MODBUS_PORT);
        if (_modbusTCPServer.begin(MODBUS_SERVER_ID)) {
            _modbusTCPServer.configureCoils(0, 4);
            _modbusTCPServer.configureDiscreteInputs(0, 6); 
            _modbusTCPServer.configureHoldingRegisters(0, 16);
            _modbusTCPServer.configureInputRegisters(0, 19);
            Log.info("Modbus TCP server started\n");
            success = true;
        } else {
            network->close();
            Log.error("Failed to start Modbus TCP server!\n");
        }
    } else {
        Log.error("Network not available. Cannot start Modbus TCP server!\n");
    }
    return success;
}

/**
 * @brief Synchronizes telemetry into registers, handles clients, and processes incoming commands.
 * 
 * @param config Reference to system configuration struct.
 * @param state Reference to current machine state struct.
 * @param isSaveConfiguration Reference to save command flag.
 * @param isClearAlarms Reference to alarm clear flag.
 * @param isSystemReset Reference to system reboot flag.
 */
void ModbusService::update(Configuration& config, MachineState& state, bool& isSaveConfiguration, bool& isClearAlarms, bool& isSystemReset) {
    // -------------------------------------------------------------------------
    // 1. Publish internal machine states to Modbus registers
    // -------------------------------------------------------------------------

    // Update command coils
    _modbusTCPServer.coilWrite(0, (uint8_t) isSaveConfiguration);
    _modbusTCPServer.coilWrite(1, (uint8_t) isClearAlarms);
    _modbusTCPServer.coilWrite(2, (uint8_t) isSystemReset);
    _modbusTCPServer.coilWrite(3, (uint8_t) config.isDhcp);

    // Update discrete digital inputs
    _modbusTCPServer.discreteInputWrite(0, (uint8_t) state.valve->isActive());
    _modbusTCPServer.discreteInputWrite(1, (uint8_t) state.alarm->isActive());
    _modbusTCPServer.discreteInputWrite(2, (uint8_t) state.isRinsing);
    _modbusTCPServer.discreteInputWrite(3, (uint8_t) config.isDhcp);
    _modbusTCPServer.discreteInputWrite(4, (uint8_t) (state.alarmCode & ALARM_PROBE));
    _modbusTCPServer.discreteInputWrite(5, (uint8_t) (state.alarmCode & ALARM_VALVE));

    // Update holding registers (mirrors configuration to allow change detection on client write)
    _modbusTCPServer.holdingRegisterWrite(0, (uint16_t) config.probeMaxHeight_cm);
    _modbusTCPServer.holdingRegisterWrite(1, (uint16_t) config.tankMaxHeight_cm);
    _modbusTCPServer.holdingRegisterWrite(2, (uint16_t) config.tankSurface_cm2);
    _modbusTCPServer.holdingRegisterWrite(3, (uint16_t) config.waterLevelLow_cm);
    _modbusTCPServer.holdingRegisterWrite(4, (uint16_t) config.waterLevelHysteresis_cm);
    _modbusTCPServer.holdingRegisterWrite(5, (uint16_t) config.valveTimeout_min);
    _modbusTCPServer.holdingRegisterWrite(6, (uint16_t) config.valveRinsePeriod_day);
    _modbusTCPServer.holdingRegisterWrite(7, (uint16_t) config.valveRinseDuration_min);
    _modbusTCPServer.holdingRegisterWrite(8, ((uint16_t) config.ipAddress[0] << 8) | config.ipAddress[1]);
    _modbusTCPServer.holdingRegisterWrite(9, ((uint16_t) config.ipAddress[2] << 8) | config.ipAddress[3]);
    _modbusTCPServer.holdingRegisterWrite(10, ((uint16_t) config.ipSubnet[0] << 8) | config.ipSubnet[1]);
    _modbusTCPServer.holdingRegisterWrite(11, ((uint16_t) config.ipSubnet[2] << 8) | config.ipSubnet[3]);
    _modbusTCPServer.holdingRegisterWrite(12, ((uint16_t) config.ipGateway[0] << 8) | config.ipGateway[1]);
    _modbusTCPServer.holdingRegisterWrite(13, ((uint16_t) config.ipGateway[2] << 8) | config.ipGateway[3]);
    _modbusTCPServer.holdingRegisterWrite(14, ((uint16_t) config.ipDNS[0] << 8) | config.ipDNS[1]);
    _modbusTCPServer.holdingRegisterWrite(15, ((uint16_t) config.ipDNS[2] << 8) | config.ipDNS[3]);

    // Update input registers (telemetry and read-only configuration copies)
    _modbusTCPServer.inputRegisterWrite(0, (uint16_t) config.probeMaxHeight_cm);
    _modbusTCPServer.inputRegisterWrite(1, (uint16_t) config.tankMaxHeight_cm);
    _modbusTCPServer.inputRegisterWrite(2, (uint16_t) config.tankSurface_cm2);
    _modbusTCPServer.inputRegisterWrite(3, (uint16_t) config.waterLevelLow_cm);
    _modbusTCPServer.inputRegisterWrite(4, (uint16_t) config.waterLevelHysteresis_cm);    
    _modbusTCPServer.inputRegisterWrite(5, (uint16_t) config.valveTimeout_min);
    _modbusTCPServer.inputRegisterWrite(6, (uint16_t) config.valveRinsePeriod_day);
    _modbusTCPServer.inputRegisterWrite(7, (uint16_t) config.valveRinseDuration_min);
    _modbusTCPServer.inputRegisterWrite(8, ((uint16_t) config.ipAddress[0] << 8) | config.ipAddress[1]);
    _modbusTCPServer.inputRegisterWrite(9, ((uint16_t) config.ipAddress[2] << 8) | config.ipAddress[3]);
    _modbusTCPServer.inputRegisterWrite(10, ((uint16_t) config.ipSubnet[0] << 8) | config.ipSubnet[1]);
    _modbusTCPServer.inputRegisterWrite(11, ((uint16_t) config.ipSubnet[2] << 8) | config.ipSubnet[3]);
    _modbusTCPServer.inputRegisterWrite(12, ((uint16_t) config.ipGateway[0] << 8) | config.ipGateway[1]);
    _modbusTCPServer.inputRegisterWrite(13, ((uint16_t) config.ipGateway[2] << 8) | config.ipGateway[3]);
    _modbusTCPServer.inputRegisterWrite(14, ((uint16_t) config.ipDNS[0] << 8) | config.ipDNS[1]);
    _modbusTCPServer.inputRegisterWrite(15, ((uint16_t) config.ipDNS[2] << 8) | config.ipDNS[3]);

    // Analog telemetry scaled to integer units:
    _modbusTCPServer.inputRegisterWrite(16, (uint16_t) (1000.0 * state.waterLevel_mA)); // Current scaled to microamperes (uA)
    _modbusTCPServer.inputRegisterWrite(17, (uint16_t) (10.0 * state.waterLevel_cm));   // Water level scaled to millimeters (mm)
    _modbusTCPServer.inputRegisterWrite(18, (uint16_t) state.probeErrorCnt);            // # of probe errors (* ALARM_PROBE_COST)

    // -------------------------------------------------------------------------
    // 2. Client Connection Management
    // -------------------------------------------------------------------------
    if (!_modbusClient || !_modbusClient.connected()) {
        if (_modbusClient) {
            _modbusClient.stop(); // Clean up disconnected client resources
            Log.info("Client disconnected.\n");
        }
        
        _modbusClient = _modbusServer->available();
        if (_modbusClient) {
            _modbusTCPServer.accept(_modbusClient);
            Log.info("Client connected.\n");
        }
    }

    // -------------------------------------------------------------------------
    // 3. Process Client Requests
    // -------------------------------------------------------------------------
    if (_modbusClient && _modbusClient.connected()) {
        _modbusTCPServer.poll(); // Process incoming Modbus TCP packets non-blockingly

        // Check command coils and dispatch actions
        if (_modbusTCPServer.coilRead(0) != 0) {
            Log.info("Save configuration requested via Modbus\n");
            isSaveConfiguration = true;
            _modbusTCPServer.coilWrite(0, 0); // Self-reset coil to 0 after execution
        }
        if (_modbusTCPServer.coilRead(1) != 0) {
            Log.info("Clear alarms requested via Modbus\n");
            isClearAlarms = true;
            _modbusTCPServer.coilWrite(1, 0); // Self-reset coil to 0 after execution
        }
        if (_modbusTCPServer.coilRead(2) != 0) {
            if (!isSystemReset) {
                Log.info("System reset requested via Modbus\n");
            }
            isSystemReset = true;
        }
        if (_modbusTCPServer.coilRead(3) != config.isDhcp) {
            config.isDhcp = !config.isDhcp;
            Log.info("DHCP updated to %s\n", config.isDhcp ? "enabled" : "disabled");
            _modbusTCPServer.coilWrite(3, config.isDhcp);
        }   

        // Read holding registers and update configuration when remote writes occur
        // FIXME: Validate config/add range checks ?
        for (int i = 0; i < 16; i++) {
            uint16_t value = (uint16_t) _modbusTCPServer.holdingRegisterRead(i);
            uint16_t value32MSB;
            IPAddress newIP;
            switch (i) {
                case 0: // Probe max height (@20mA) in cm
                    if (config.probeMaxHeight_cm != value) {
                        config.probeMaxHeight_cm = value;
                        Log.info("Probe max. height updated to %d cm\n", value);
                    }
                    break;
                case 1: // Tank overflow height in cm
                    if (config.tankMaxHeight_cm != value) {
                        config.tankMaxHeight_cm = value;
                        Log.info("Tank max. height updated to %d cm\n", value);
                    }
                    break;
                case 2: // Tank cross-sectional area in cm2
                    if (config.tankSurface_cm2 != value) {
                        config.tankSurface_cm2 = value;
                        Log.info("Tank surface updated to %d cm2\n", value);
                    }
                    break;
                case 3: // Minimum water level threshold in cm
                    if (config.waterLevelLow_cm != value) {
                        config.waterLevelLow_cm = value;
                        Log.info("Water low level updated to %d cm\n", value);
                    }
                    break;
                case 4: // Refill hysteresis in cm
                    if (config.waterLevelHysteresis_cm != value) {
                        config.waterLevelHysteresis_cm = value;
                        Log.info("Water level hysteresis updated to %d cm\n", value);
                    }
                    break;                                        
                case 5: // Valve continuous open timeout in minutes
                    if (config.valveTimeout_min != value) {
                        config.valveTimeout_min = value;
                        Log.info("Valve timeout updated to %d min\n", value);
                    }
                    break;
                case 6: // Periodic rinse period in days
                    if (config.valveRinsePeriod_day != value) {
                        config.valveRinsePeriod_day = value;
                        Log.info("Rinse period updated to %d days\n", value);
                    }
                    break;
                case 7: // Valve rinse duration in minutes
                    if (config.valveRinseDuration_min != value) {
                        config.valveRinseDuration_min = value;
                        Log.info("Rinse duration updated to %d min\n", value);
                    }
                    break;
                case 9: // Static IP address (applied when LSB word 9 is written)
                    value32MSB = (uint16_t) _modbusTCPServer.holdingRegisterRead(8);
                    newIP = IPAddress(value32MSB >> 8, value32MSB & 0xff, value >> 8, value & 0xff);
                    if (config.ipAddress != newIP) {
                        config.ipAddress = newIP;
                        Log.info("IP address updated to %d.%d.%d.%d\n", newIP[0], newIP[1], newIP[2], newIP[3]);
                    }
                    break;
                case 11: // Subnet mask (applied when LSB word 11 is written)
                    value32MSB = (uint16_t) _modbusTCPServer.holdingRegisterRead(10);
                    newIP = IPAddress(value32MSB >> 8, value32MSB & 0xff, value >> 8, value & 0xff);
                    if (config.ipSubnet != newIP) {
                        config.ipSubnet = newIP;
                        Log.info("Subnet updated to %d.%d.%d.%d\n", newIP[0], newIP[1], newIP[2], newIP[3]);
                    }
                    break;
                case 13: // Gateway IP address (applied when LSB word 13 is written)
                    value32MSB = (uint16_t) _modbusTCPServer.holdingRegisterRead(12);
                    newIP = IPAddress(value32MSB >> 8, value32MSB & 0xff, value >> 8, value & 0xff);
                    if (config.ipGateway != newIP) {
                        config.ipGateway = newIP;
                        Log.info("Gateway address updated to %d.%d.%d.%d\n", newIP[0], newIP[1], newIP[2], newIP[3]);
                    }
                    break;
                case 15: // DNS server IP address (applied when LSB word 15 is written)
                    value32MSB = (uint16_t) _modbusTCPServer.holdingRegisterRead(14);
                    newIP = IPAddress(value32MSB >> 8, value32MSB & 0xff, value >> 8, value & 0xff);
                    if (config.ipDNS != newIP) {
                        config.ipDNS = newIP;
                        Log.info("DNS address updated to %d.%d.%d.%d\n", newIP[0], newIP[1], newIP[2], newIP[3]);
                    }
                    break;
            }
        }
    }
}
