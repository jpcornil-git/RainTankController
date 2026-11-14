/**
 * @file Network.cpp
 * @brief Implementation of Ethernet network initialization and IP management.
 */

#include <Arduino.h>
#include <ArduinoLog.h>
#include <SPI.h>
#include <utility/w5500.h>
#include "Network.h"

/**
 * @brief Constructs a new Network manager instance with network disabled.
 */
Network::Network() {
    _isNetworkAvailable = false;
}

/**
 * @brief Checks if Ethernet hardware is initialized and available.
 * 
 * @return bool True if network is operational, false otherwise.
 */
bool Network::isAvailable() {
    return _isNetworkAvailable;
}

/**
 * @brief Closes network availability flag.
 */
void Network::close() {
    if (_isNetworkAvailable) {
        _isNetworkAvailable = false;
    }
}

/**
 * @brief Derives deterministic/randomized octets for RFC 3927 Link-Local address (169.254.x.x).
 * 
 * Hashes the 6-byte MAC address, seeds the random generator with the hash plus
 * noise read from floating analog pin A13, and generates valid host octets.
 * 
 * @param ipByte3 Pointer to receive 3rd octet (1..254).
 * @param ipByte4 Pointer to receive 4th octet (0..255).
 */
void Network::hashMAC(byte *ipByte3, byte *ipByte4) {
    uint16_t hash = 0;
    for (int i = 0; i < 6; i++) {
        hash += _mac[i];
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }
    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);

    // Add entropy from analog pin A13 (floating on IND.I/O board)
    randomSeed(hash + analogRead(A13)); 

    *ipByte3 = random(1, 255); // RFC 3927: Avoid 0 and 255 for the first host byte
    *ipByte4 = random(0, 256);
}

/**
 * @brief Reads the hardware factory MAC address from the RTC EEPROM.
 * 
 * The RTC chip stores an 8-byte EUI-64 starting at address 0xF0.
 * Bytes 0..2 represent the OUI, and bytes 5..7 represent the device extension.
 * Skipping bytes 3 and 4 yields the standard 48-bit MAC address.
 * 
 * @param i2c Pointer to I2c bus helper.
 */
void Network::readMACfromRTC(I2c *i2c) {
    Log.info("READING MAC address from RTC EEPROM\n");
    int mac_index = 0;
    // Read 8 bytes of 64-bit MAC address: 3 bytes valid OUI, 5 bytes unique EI
    for (int i = 0; i < 8; i++) {
        // For 6-byte MAC, skip middle 2 bytes of 64-bit EUI
        if (i != 3 && i != 4) {
            byte regValue;
            if (!i2c->readByte(0x57, 0xf0 + i, &regValue)) {
                Log.error(" Failed to read from RTC\n");
                break;
            }
            _mac[mac_index] = regValue;
            mac_index++;
        }      
    }
}

/**
 * @brief Initializes SPI bus and checks W5500 Ethernet controller presence.
 * 
 * Reads the W5500 version register (expected 0x04). If valid, retrieves MAC
 * from RTC EEPROM and sets network availability.
 * 
 * @param i2c Pointer to initialized I2c helper.
 */
void Network::init(I2c *i2c) {
    _isNetworkAvailable = false;
    _dhcpMode = DhcpMode::UNKNOWN;

    SPI.begin();
    w5500.init();
    uint8_t chipVersion = w5500.readVersion();
    if (chipVersion != 0x04) {
        Log.warning("Error: Ethernet shield not found or not W5500 (read %X)\n", chipVersion);
    } else {
        Log.info("Ethernet shield W5500 found\n");
        // Read MAC address stored in RTC EEPROM
        if (i2c != nullptr) readMACfromRTC(i2c);
        _isNetworkAvailable = true;
    }
}

/**
 * @brief Configures IP address either via static assignment or DHCP with auto-IP fallback.
 * 
 * If a static IP is given, Ethernet.begin is called directly.
 * Otherwise, DHCP is attempted (60s timeout). On DHCP failure, a Link-Local
 * fallback IP (169.254.x.x) is derived using hashMAC.
 * 
 * @param ip Static IP address or INADDR_NONE for DHCP.
 * @param dns DNS server IP address.
 * @param gateway Gateway router IP address.
 * @param subnet Subnet mask.
 */
void Network::begin(IPAddress ip, IPAddress dns, IPAddress gateway, IPAddress subnet) {
    if (ip != INADDR_NONE) {
        // Use static IP address configuration
        Ethernet.begin(_mac, ip, dns, gateway, subnet);
        _dhcpMode = DhcpMode::STATIC;
    } else {
        // Use DHCP to obtain IP address
        Log.info("Attempting to obtain IP address via DHCP...");
        if (Ethernet.begin(_mac)) { // 60s timeout for DHCP
            Log.info("Success\n");
            _dhcpMode = DhcpMode::DHCP;
        } else {
            // DHCP failed, use Link-Local address (169.254.x.x)
            byte ipByte3, ipByte4;

            Log.warning("Failed, using Link-Local address\n");
            _dhcpMode = DhcpMode::DHCP_FAILED;

            hashMAC(&ipByte3, &ipByte4);
            IPAddress autoIP(169, 254, ipByte3, ipByte4);
            IPAddress subnet(255, 255, 0, 0);

            Ethernet.begin(_mac, autoIP, INADDR_NONE, INADDR_NONE, subnet); // No DNS or gateway for Link-Local
        }
    }

    IPAddress local_ip = Ethernet.localIP();
    char macAddressString[18];
    snprintf(macAddressString,
        sizeof(macAddressString), "%02X:%02X:%02X:%02X:%02X:%02X",
        _mac[0], _mac[1], _mac[2], _mac[3], _mac[4], _mac[5]
    );
    Log.info("Network initialized with MAC %s and IP %d.%d.%d.%d\n",
        macAddressString,
        local_ip[0], local_ip[1], local_ip[2], local_ip[3]
    );
}

/**
 * @brief Update network settings
 * Handle DHCP renewal and rebinding
 */
void Network::update() {

    if (_dhcpMode != DhcpMode::DHCP) return;

    const char *msg = nullptr;
    switch (Ethernet.maintain()) {
        case DHCP_CHECK_RENEW_FAIL:
            msg = "Renewal failed";
            break;
        case DHCP_CHECK_RENEW_OK:
            msg = "Renewal success";
            break;
        case DHCP_CHECK_REBIND_FAIL:
            msg = "Rebinding failed";
            break;
        case DHCP_CHECK_REBIND_OK:
            msg = "Rebinding success"; 
            break;
    }
    if (msg != nullptr) {
        IPAddress current_ip = Ethernet.localIP();
        Log.info("DHCP: %s, actual IP %d.%d.%d.%d\n",
            msg,
            current_ip[0], current_ip[1], current_ip[2], current_ip[3]
        );
    }
}