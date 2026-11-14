#ifndef I2C_H
#define I2C_H

#include <stdint.h>

/**
 * @file I2c.h
 * @brief Helper utility class for I2C bus initialization and register reading.
 */

class I2c {
public:
    /**
     * @brief Construct a new I2c instance and initialize Wire library.
     */
    I2c();

    /**
     * @brief Default destructor.
     */
    ~I2c() = default;

    /**
     * @brief Read a single byte from a specified I2C device register.
     * 
     * Performs a write-then-read transaction with a repeated start condition
     * (endTransmission without STOP) to access target register contents.
     * 
     * @param i2cAddr 7-bit I2C device slave address.
     * @param regAddr 8-bit internal register address to read.
     * @param regValue Pointer to store the retrieved byte.
     * @return bool True if read succeeded, false on bus error or timeout.
     */
    static bool readByte(uint8_t i2cAddr, uint8_t regAddr, uint8_t *regValue);
};

#endif // I2C_H
