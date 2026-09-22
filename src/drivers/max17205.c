/**
 * @authors Ailisi Bao, John Buttles, Jacob Rohozen
 * @file max17205.c
 * 
 * @brief This file defines the implementation for reading certain registers of the MAX17205 fuel gauge 
 * on the Argus mainboard. This utilizes the defined I2C communication driver in include/argus and src/board. 
 * 
 */

#include "argus/drivers/max17205.h"
#include <stdio.h>

/* Defining Internal Addresses for Reads on the MAX17205 */
#define MAX17205_VCELL_ADDR 0x09  // Lowest cell voltage of a pack
#define MAX17205_REPSOC_ADDR 0x06  // Reported state of charge
#define MAX17205_REPCAP_ADDR 0x05  // Reported remaining capacity
#define MAX17205_CURRENT_ADDR 0x0A  // Battery current
#define MAX17205_TTE_ADDR 0x11  // Time to empty
#define MAX17205_TTF_ADDR 0x20  // Time to full
#define MAX17205_CAPACITY_ADDR 0x10  // Full capacity estimation
#define MAX17205_VBAT_ADDR 0xDA  // Battery pack voltage
#define MAX17205_AVCELL_ADDR 0x17  // Battery cycles
#define MAX17205_TIMERH_ADDR 0xBE  // Time since power up
#define MAX17205_TEMP_ADDR 0x08  // Temp register

#define MAX17205_COMMAND_ADDR 0x60  // Command register
#define MAX17205_CONFIG2_ADDR 0xBB  // Command register

/* Addresses in shadow RAM (I2C address 0x0B) */
#define MAX17205_TEMP1_ADDR 0x34  // AIN1 thermistor temperature
#define MAX17205_TEMP2_ADDR 0x3B  // AIN2 thermistor temperature
#define MAX17205_INTTEMP_ADDR 0x35  // Internal die temperature

/* Init/Deinit */
max17205_status_t max17205_init(max17205_t *dev, i2c_bus_t *bus) {
    if (dev == NULL || bus == NULL) {return max17205_arg_err;}
    if (dev->init) {return max17205_arg_err;}
    if (!bus->init) {return max17205_arg_err;}

    if (i2c_device_init(&dev->main, bus, 0x36) != i2c_ok) {
        return max17205_arg_err;
    }

    if (i2c_device_init(&dev->shadow, bus, 0x0B) != i2c_ok) {
        return max17205_arg_err;
    }

    dev->init = true;
    return max17205_ok;
}

max17205_status_t max17205_deinit(max17205_t *dev) {
    return max17205_bad;
}

/* Read operations */
max17205_status_t max17205_read_soc(max17205_t *dev, max17205_readings_t *val) {
    return max17205_bad;
}

max17205_status_t max17205_read_capacity(max17205_t *dev, max17205_readings_t *val) {
    return max17205_bad;
}

max17205_status_t max17205_read_current(max17205_t *dev, max17205_readings_t *val) {
    return max17205_bad;
}

max17205_status_t max17205_read_voltage(max17205_t *dev, max17205_readings_t *val) {
    return max17205_bad;
}

max17205_status_t max17205_read_midvoltage(max17205_t *dev, max17205_readings_t *val) {
    return max17205_bad;
}

max17205_status_t max17205_read_cycles(max17205_t *dev, max17205_readings_t *val) {
    return max17205_bad;
}

max17205_status_t max17205_read_tte(max17205_t *dev, max17205_readings_t *val) {
    return max17205_bad;
}

max17205_status_t max17205_read_ttf(max17205_t *dev, max17205_readings_t *val) {
    return max17205_bad;
}

max17205_status_t max17205_read_time_pwrup(max17205_t *dev, max17205_readings_t *val) {
    return max17205_bad;
}

max17205_status_t max17205_read_temperature(max17205_t *dev, max17205_readings_t *val) {
    return max17205_bad;
}

max17205_status_t max17205_read_temperature_ain1(max17205_t *dev, max17205_readings_t *val) {
    return max17205_bad;
}

max17205_status_t max17205_read_temperature_ain2(max17205_t *dev, max17205_readings_t *val) {
    return max17205_bad;
}

max17205_status_t max17205_read_temperature_die(max17205_t *dev, max17205_readings_t *val) {
    return max17205_bad;
}

/* Read all operation */
max17205_status_t max17205_read_all(max17205_t *dev, max17205_readings_t *val) {
    return max17205_bad;
}

/* Reset */
max17205_status_t max17205_reset(max17205_t *dev) {
    return max17205_bad;
}
