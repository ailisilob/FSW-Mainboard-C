/**
 * @authors Ailisi Bao, John Buttles, Jacob Rohozen
 * @file max17205.h
 * 
 * @brief This file defines the interface for the MAX17205 fuel gauge on the Argus mainboard.
 * This utilizes the defined I2C communication driver in include/argus and src/board. 
 * 
 */

#ifndef MAX17205
#define MAX17205

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <float.h>
#include "pico/float.h"
#include "argus/i2c_bus.h"

typedef enum {
    max17205_ok = 0,
    max17205_arg_err,
    max17205_not_init
} max17205_status_t;

typedef struct {
    i2c_device_t main;
    i2c_device_t shadow;
    bool init;
} max17205_t;

typedef struct {
    uint32_t soc;
    uint32_t capacity;
    int32_t current;
    uint32_t voltage;
    uint32_t midvoltage;
    uint16_t cycles;
    uint16_t tte;
    uint16_t ttf;
    uint16_t time_pwrup;
    int16_t temperature;
    int16_t temperature_ain1;
    int16_t temperature_ain2;
    int16_t temperature_die;
} max17205_readings_t;

/* Init/Deinit */
max17205_status_t max17205_init(max17205_t *dev, i2c_bus_t *bus);
max17205_status_t max17205_deinit(max17205_t *dev);

/* Read operations */
max17205_status_t max17205_read_soc(max17205_t *dev, max17205_readings_t *val);
max17205_status_t max17205_read_capacity(max17205_t *dev, max17205_readings_t *val);
max17205_status_t max17205_read_current(max17205_t *dev, max17205_readings_t *val);
max17205_status_t max17205_read_voltage(max17205_t *dev, max17205_readings_t *val);
max17205_status_t max17205_read_midvoltage(max17205_t *dev, max17205_readings_t *val);
max17205_status_t max17205_read_cycles(max17205_t *dev, max17205_readings_t *val);
max17205_status_t max17205_read_tte(max17205_t *dev, max17205_readings_t *val);
max17205_status_t max17205_read_ttf(max17205_t *dev, max17205_readings_t *val);
max17205_status_t max17205_read_time_pwrup(max17205_t *dev, max17205_readings_t *val);
max17205_status_t max17205_read_temperature(max17205_t *dev, max17205_readings_t *val);
max17205_status_t max17205_read_temperature_ain1(max17205_t *dev, max17205_readings_t *val);
max17205_status_t max17205_read_temperature_ain2(max17205_t *dev, max17205_readings_t *val);
max17205_status_t max17205_read_temperature_die(max17205_t *dev, max17205_readings_t *val);

/* Read all operation */
max17205_status_t max17205_read_all(max17205_t *dev, max17205_readings_t *val);

/* Reset */
max17205_status_t max17205_reset(max17205_t *dev);

#endif
