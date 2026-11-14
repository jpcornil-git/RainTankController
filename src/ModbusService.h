#ifndef MODBUS_SERVICE_H
#define MODBUS_SERVICE_H

/**
 * @file ModbusService.h
 * @brief Modbus TCP server interface for remote monitoring and configuration.
 * 
 * Exposes system telemetry (water level, relay status, alarms) via discrete
 * inputs and input registers, and accepts remote commands and configuration
 * changes via coils and holding registers over standard Modbus TCP (port 502).
 */

#include <ArduinoModbus.h>

#include "../include/config.h"
#include "Network.h"

class ModbusService {
public:
    static const int MODBUS_PORT = 502;      ///< Standard Modbus TCP communication port.
    static const int MODBUS_SERVER_ID = 1;   ///< Modbus slave/server unit identifier.

    /**
     * @brief Construct a new ModbusService instance.
     */
    ModbusService();

    /**
     * @brief Default destructor.
     */
    ~ModbusService() = default;

    /**
     * @brief Start the Modbus TCP server and allocate registers.
     * 
     * Configures coils, discrete inputs, holding registers, and input registers.
     * 
     * @param network Pointer to initialized Network object.
     * @return bool True if Modbus server started successfully, false otherwise.
     */
    bool begin(Network *network);

    /**
     * @brief Synchronize internal states with Modbus registers and handle client requests.
     * 
     * 1. Publishes current system state and measurements into registers.
     * 2. Accepts new client connections and polls pending requests.
     * 3. Dispatches coil and register write requests back into system state and configuration.
     * 
     * @param config Reference to system configuration struct.
     * @param state Reference to current machine state struct.
     * @param isSaveConfiguration Reference to flag triggering configuration persistence.
     * @param isClearAlarms Reference to flag triggering alarm clearance.
     * @param isSystemReset Reference to flag triggering system watchdog reset.
     */
    void update(Configuration& config, MachineState& state, bool& isSaveConfiguration, bool& isClearAlarms, bool& isSystemReset);

private:
    EthernetServer *_modbusServer = nullptr; ///< Ethernet server listening on MODBUS_PORT.
    ModbusTCPServer _modbusTCPServer;         ///< Modbus TCP server protocol handler.
    EthernetClient _modbusClient;            ///< Active Modbus TCP client connection.
};

#endif // MODBUS_SERVICE_H
