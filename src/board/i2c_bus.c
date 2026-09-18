/**
 * @authors Ailisi Bao, John Buttles, Jacob Rohozen
 * @file i2c_bus.h
 * 
 * @brief This file defines the implementation for the I2C bus on the Argus mainboard.
 * This implementation wraps the Raspberry Pi Pico (RP2350) hardware API "hardware_i2c".
 * There is little functional difference between the SDK and this implementation. The only
 * added functionality is the ability to store devices/buses and their parameters in structs.
 */

#include "argus/i2c_bus.h"