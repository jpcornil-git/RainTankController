/**
 * @file I2c.cpp
 * @brief Implementation of I2C bus transactions and error logging.
 */

#include <ArduinoLog.h>
#include <Wire.h>
#include "I2c.h"

/**
 * @brief Constructs the I2c helper and initializes the Arduino Wire library as bus master.
 */
I2c::I2c() {
    Wire.begin();
}

/**
 * @brief Reads a single byte from a specified register of an I2C device.
 * 
 * Protocol steps:
 * 1. Transmit slave address with write bit and send target register address.
 * 2. Send repeated START condition (endTransmission(false)).
 * 3. Request 1 byte from the device (requestFrom).
 * 4. Verify reception and assign to output pointer.
 * 
 * @param i2cAddr 7-bit slave address.
 * @param regAddr 8-bit register pointer.
 * @param regValue Output pointer to receive the read byte.
 * @return bool True if successful, false on transmission failure or missing data.
 */
bool I2c::readByte(uint8_t i2cAddr, uint8_t regAddr, uint8_t *regValue) {
    Wire.beginTransmission(i2cAddr);
    Wire.write(regAddr);
    byte error = Wire.endTransmission(false); // Repeated start: don't send stop
    if (error != 0) {
        Log.error("I2C read error (0x%02X): %d\n", i2cAddr, error);
        return false;
    }
    Wire.requestFrom(i2cAddr, (uint8_t)1);
    int c = Wire.read();
    if (c < 0) {
        Log.error("I2C read error (0x%02X): failed to read data from register 0x%02X\n", i2cAddr, regAddr);
        return false;
    }
    *regValue = uint8_t(c & 0xFF);
    return true;
}
