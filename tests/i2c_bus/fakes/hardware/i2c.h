/**
 * @file hardware/i2c.h
 * @brief Host-side stand-in for the Pico SDK's hardware/i2c.h.
 *
 * Signatures match pico-sdk 2.3.1 exactly so that a mismatch in i2c_bus.c
 * (wrong pointer level, wrong argument order) shows up as a compiler
 * diagnostic here rather than only on hardware.
 */

#ifndef FAKE_HARDWARE_I2C_H
#define FAKE_HARDWARE_I2C_H

#include "pico/stdlib.h"

typedef struct i2c_inst {
    int hw_index; /* 0 or 1, so tests can identify the instance in a failure message */
} i2c_inst_t;

extern i2c_inst_t i2c0_inst;
extern i2c_inst_t i2c1_inst;

#define i2c0 (&i2c0_inst)
#define i2c1 (&i2c1_inst)

/* The board header picks the default instance; the feather RP2350 uses i2c0. */
#define i2c_default i2c0

uint i2c_init(i2c_inst_t *i2c, uint baudrate);
void i2c_deinit(i2c_inst_t *i2c);
uint i2c_set_baudrate(i2c_inst_t *i2c, uint baudrate);
uint i2c_hw_index(i2c_inst_t *i2c);
/* `addr` is the address THIS controller answers to once slave is true,
 * not the address of a peer on the bus. */
void i2c_set_slave_mode(i2c_inst_t *i2c, bool slave, uint8_t addr);

int i2c_write_blocking(i2c_inst_t *i2c, uint8_t addr, const uint8_t *src, size_t len, bool nostop);
int i2c_read_blocking(i2c_inst_t *i2c, uint8_t addr, uint8_t *dst, size_t len, bool nostop);

int i2c_write_timeout_us(i2c_inst_t *i2c, uint8_t addr, const uint8_t *src, size_t len, bool nostop,
                         uint timeout_us);
int i2c_read_timeout_us(i2c_inst_t *i2c, uint8_t addr, uint8_t *dst, size_t len, bool nostop,
                        uint timeout_us);

#endif /* FAKE_HARDWARE_I2C_H */
