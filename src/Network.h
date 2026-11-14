#ifndef NETWORK_H
#define NETWORK_H

/**
 * @file Network.h
 * @brief Ethernet network manager for Industruino with W5500 controller.
 * 
 * Supports auto-detection of the optional W5500 Ethernet module, reading factory
 * MAC address from RTC EEPROM, DHCP IP leasing, Link-Local auto-IP fallback (RFC 3927),
 * and static IP configuration.
 * 
 * @note Requires the Industruino fork of the Ethernet2 library to ensure the SPI
 *       bus clock rate is configured to supported speeds on Industruino SAMD21 hardware.
 */

#include <Ethernet2.h>
#include "I2c.h"

class Network {
public:
    /**
     * @brief DHCP Operating mode.
     */
    enum class DhcpMode {
        UNKNOWN, ///< DHCP not yet started
        STATIC, ///< Static IP address.
        DHCP, ///< DHCP Success.
        DHCP_FAILED, ///< DHCP Failed, fold back to link local address
    };    
    /**
     * @brief Construct a new Network instance.
     */
    Network();

    /**
     * @brief Default destructor.
     */
    ~Network() = default;

    /**
     * @brief Detect and initialize the W5500 Ethernet controller and read MAC from RTC.
     * 
     * Verifies W5500 SPI communication and reads the hardware MAC from the onboard
     * RTC EEPROM via I2C if available.
     * 
     * @param i2c Pointer to initialized I2c helper object (or nullptr to use default MAC).
     */
    void init(I2c *i2c);

    /**
     * @brief Configure network parameters and bring up the Ethernet interface.
     * 
     * If a static IP is provided, configures static networking.
     * If ip is INADDR_NONE, attempts DHCP acquisition. If DHCP fails,
     * automatically generates a Link-Local (169.254.x.x) address.
     * 
     * @param ip Static IP address or INADDR_NONE for DHCP.
     * @param dns DNS server IP address (optional).
     * @param gateway Gateway IP address (optional).
     * @param subnet Subnet mask (optional).
     */
    void begin(IPAddress ip, IPAddress dns = INADDR_NONE, IPAddress gateway = INADDR_NONE, IPAddress subnet = INADDR_NONE);

    /**
     * @brief Update network settings
     */
    void update();

    /**
     * @brief Disconnect and mark network as unavailable.
     */
    void close();

    /**
     * @brief Check whether network hardware is present and operational.
     * 
     * @return bool True if W5500 is initialized and ready, false otherwise.
     */
    bool isAvailable();

    /**
     * @brief Get current DHCP mode.
     * 
     * @return DhcpMode enum.
     */
    DhcpMode getDhcpMode() { return _dhcpMode; }

    /**
     * @brief Get currently assigned local IP address.
     * @return IPAddress Current IP address.
     */
    IPAddress getIP() { return Ethernet.localIP(); }

    /**
     * @brief Get currently configured network gateway address.
     * @return IPAddress Current gateway IP address.
     */
    IPAddress getGateway() { return Ethernet.gatewayIP(); }

    /**
     * @brief Get currently configured subnet mask.
     * @return IPAddress Current subnet mask.
     */
    IPAddress getSubnet() { return Ethernet.subnetMask(); }

    /**
     * @brief Get currently configured DNS server address.
     * @return IPAddress Current DNS server IP address.
     */
    IPAddress getDNS() { return Ethernet.dnsServerIP(); }

private:
    /**
     * @brief Read 48-bit MAC address from RTC EEPROM over I2C.
     * 
     * Reads the 64-bit EUI from the RTC chip (address 0x57, regs 0xF0..0xF7)
     * and converts it to a standard 6-byte Ethernet MAC address by skipping bytes 3 and 4.
     * 
     * @param i2c Pointer to I2c helper instance.
     */
    void readMACfromRTC(I2c *i2c);

    /**
     * @brief Generate pseudo-random octets for a 169.254.x.x Link-Local IP address.
     * 
     * Hashes the MAC address and mixes entropy from floating analog pin A13.
     * Complies with RFC 3927 by avoiding 0 and 255 for the first host octet.
     * 
     * @param ipByte3 Pointer to receive 3rd IP octet (1..254).
     * @param ipByte4 Pointer to receive 4th IP octet (0..255).
     */
    void hashMAC(byte *ipByte3, byte *ipByte4);

    bool _isNetworkAvailable;                                    ///< Flag indicating active network availability.
    byte _mac[6] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x01};         ///< Default locally administered MAC address.
    DhcpMode _dhcpMode;                                          ///< Current DHCP mode 
};
#endif // NETWORK_H
