/**
 * @authors Ailisi Bao, John Buttles, Jacob Rohozen
 * @file i2c_bus.h
 * 
 * @brief This file defines the interface for the I2C bus on the Argus mainboard.
 * This interface wraps the Raspberry Pi Pico (RP2350) hardware API "hardware_i2c"
 * into its own functionality in src/board/i2c_bus.c.
 * 
 */

#ifndef I2C_BUS
#define I2C_BUS

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "hardware/i2c.h"
#include "pico/sync.h"
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#endif

/* I2C buses on mainboard do not correspond to default values defined by SDK */
#define I2C_SDA_0 24
#define I2C_SCL_0 25
#define I2C_SDA_1 46
#define I2C_SCL_1 47

/* Defining baud rate of I2C buses (fast mode) */
#define I2C_baud 400000
/* Defining time out of bus in case device pulls bus too long */
#define I2C_timeout 10000

/* Defining bus id to pass into bus init */
typedef enum {
    i2c_0 = 0,
    i2c_1,
    bus_count
} i2c_bus_id_t;

/* Defining status code for operations on bus and device */
typedef enum {
    i2c_ok = 0,
    i2c_arg_err,
    i2c_bus_not_init,
    i2c_dev_not_init,
    i2c_nack_err,
    i2c_timeout,
    i2c_busy,
} i2c_status_t;

/* Defining I2C bus */
typedef struct {
    i2c_inst_t *i2c; // either i2c0 or i2c1
    uint32_t baudrate; // returned by i2c_init (SDK)
    uint32_t timeout; // defaults to I2C_timeout
    uint sda;
    uint scl;
    bool init;
    mutex_t lock;
} i2c_bus_t;

/* Defining device on the I2C bus */
typedef struct {
    i2c_bus_t *bus;
    uint8_t addr;
} i2c_device_t;

/* Bus bring-up & operations */
i2c_status_t i2c_bus_init(i2c_bus_t *bus, i2c_bus_id_t id, uint32_t baud, uint32_t time);
void i2c_bus_deinit(i2c_bus_t *bus);
i2c_status_t i2c_bus_recover(i2c_bus_t *bus);
i2c_status_t i2c_bus_lock(i2c_bus_t *bus);
void i2c_bus_unlock(i2c_bus_t *bus);

/* Bus settings */
i2c_status_t i2c_set_baud(i2c_bus_t *bus);
i2c_status_t i2c_set_mode(i2c_bus_t *bus);

/* Device operations */
i2c_status_t i2c_device_init(i2c_device_t *dev, i2c_bus_t *bus, uint8_t addr);
i2c_status_t i2c_read(i2c_device_t *dev, uint8_t *dst, size_t len);
i2c_status_t i2c_write(i2c_device_t *dev, const uint8_t *src, size_t len);
i2c_status_t ic2_write_read(i2c_device_t *dev, const uint8_t *src, uint8_t *dst, size_t len);

/* Helper fxn for scanning devices on bus */
i2c_status_t scan_bus(i2c_bus_t *bus);
