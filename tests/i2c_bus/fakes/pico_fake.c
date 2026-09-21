/**
 * @file pico_fake.c
 * @brief Implementation of the faked Pico SDK surface.
 */

#include "pico_fake.h"

#include <string.h>

pico_fake_t pico_fake;

i2c_inst_t i2c0_inst = {.hw_index = 0};
i2c_inst_t i2c1_inst = {.hw_index = 1};

void pico_fake_reset(void) {
    memset(&pico_fake, 0, sizeof(pico_fake));
    /* i2c_init reports the baud it actually achieved; default to "exactly what
     * was asked for" so tests that do not care can ignore it. */
    pico_fake.i2c_init_return = 0;
    pico_fake.read_fill = 0xA5;
}

void pico_fake_set_ack(uint8_t addr, bool acks) {
    if (addr < 128) {
        pico_fake.ack[addr] = acks;
    }
}

/* Shared body for the four transfer entry points. */
static int record_xfer(i2c_inst_t *i2c, uint8_t addr, const uint8_t *src, uint8_t *dst, size_t len,
                       bool nostop, uint timeout_us, bool timed_variant) {
    int ret;

    if (pico_fake.force_ret) {
        ret = pico_fake.forced_ret;
    } else if (addr < 128 && pico_fake.ack[addr]) {
        ret = (int)len;
    } else {
        ret = PICO_ERROR_GENERIC;
    }

    if (dst != NULL && ret > 0) {
        memset(dst, pico_fake.read_fill, len);
    }

    if (pico_fake.xfer_calls < PICO_FAKE_MAX_EVENTS) {
        pico_fake.xfer[pico_fake.xfer_calls] = (pico_fake_xfer_t){
            .inst = i2c,
            .addr = addr,
            .src = src,
            .dst = dst,
            .len = len,
            .nostop = nostop,
            .timeout_us = timeout_us,
            .timed_variant = timed_variant,
            .ret = ret,
        };
    }
    pico_fake.xfer_calls++;
    if (dst != NULL) {
        pico_fake.read_calls++;
    } else {
        pico_fake.write_calls++;
    }
    return ret;
}

/* ---------------------------------------------------------------- i2c ---- */

uint i2c_init(i2c_inst_t *i2c, uint baudrate) {
    pico_fake.i2c_init_calls++;
    pico_fake.i2c_init_inst = i2c;
    pico_fake.i2c_init_baud = baudrate;
    /* The real i2c_init returns the baud rate it could actually set. */
    return pico_fake.i2c_init_return != 0 ? pico_fake.i2c_init_return : baudrate;
}

void i2c_deinit(i2c_inst_t *i2c) {
    pico_fake.i2c_deinit_calls++;
    pico_fake.i2c_deinit_inst = i2c;
}

uint i2c_set_baudrate(i2c_inst_t *i2c, uint baudrate) {
    pico_fake.i2c_set_baudrate_calls++;
    pico_fake.i2c_set_baudrate_inst = i2c;
    pico_fake.i2c_set_baudrate_baud = baudrate;
    return pico_fake.i2c_set_baudrate_return != 0 ? pico_fake.i2c_set_baudrate_return : baudrate;
}

void i2c_set_slave_mode(i2c_inst_t *i2c, bool slave, uint8_t addr) {
    pico_fake.i2c_set_slave_mode_calls++;
    pico_fake.i2c_set_slave_mode_inst = i2c;
    pico_fake.i2c_set_slave_mode_slave = slave;
    pico_fake.i2c_set_slave_mode_addr = addr;
}

uint i2c_hw_index(i2c_inst_t *i2c) {
    return i2c == i2c1 ? 1u : 0u;
}

int i2c_write_blocking(i2c_inst_t *i2c, uint8_t addr, const uint8_t *src, size_t len, bool nostop) {
    return record_xfer(i2c, addr, src, NULL, len, nostop, 0, false);
}

int i2c_read_blocking(i2c_inst_t *i2c, uint8_t addr, uint8_t *dst, size_t len, bool nostop) {
    return record_xfer(i2c, addr, NULL, dst, len, nostop, 0, false);
}

int i2c_write_timeout_us(i2c_inst_t *i2c, uint8_t addr, const uint8_t *src, size_t len, bool nostop,
                         uint timeout_us) {
    return record_xfer(i2c, addr, src, NULL, len, nostop, timeout_us, true);
}

int i2c_read_timeout_us(i2c_inst_t *i2c, uint8_t addr, uint8_t *dst, size_t len, bool nostop,
                        uint timeout_us) {
    return record_xfer(i2c, addr, NULL, dst, len, nostop, timeout_us, true);
}

/* --------------------------------------------------------------- gpio ---- */

void gpio_set_function(uint gpio, gpio_function_t fn) {
    if (pico_fake.gpio_set_function_calls < PICO_FAKE_MAX_EVENTS) {
        pico_fake.gpio_set_function[pico_fake.gpio_set_function_calls] =
            (pico_fake_gpio_fn_t){.gpio = gpio, .fn = fn};
    }
    pico_fake.gpio_set_function_calls++;
}

void gpio_pull_up(uint gpio) {
    if (pico_fake.gpio_pull_up_calls < PICO_FAKE_MAX_EVENTS) {
        pico_fake.gpio_pull_up_pin[pico_fake.gpio_pull_up_calls] = gpio;
    }
    pico_fake.gpio_pull_up_calls++;
}

void gpio_disable_pulls(uint gpio) {
    if (pico_fake.gpio_disable_pulls_calls < PICO_FAKE_MAX_EVENTS) {
        pico_fake.gpio_disable_pulls_pin[pico_fake.gpio_disable_pulls_calls] = gpio;
    }
    pico_fake.gpio_disable_pulls_calls++;
}

void gpio_init(uint gpio) {
    (void)gpio;
}

void gpio_set_dir(uint gpio, bool out) {
    (void)gpio;
    (void)out;
    pico_fake.gpio_set_dir_calls++;
}

void gpio_put(uint gpio, bool value) {
    if (gpio < 64) {
        pico_fake.gpio_level[gpio] = value;
    }
    pico_fake.gpio_put_calls++;
}

bool gpio_get(uint gpio) {
    /* Idle-high, matching a bus held up by its pull-ups. */
    return gpio < 64 ? pico_fake.gpio_level[gpio] : true;
}

/* -------------------------------------------------------------- mutex ---- */

void mutex_init(mutex_t *mtx) {
    pico_fake.mutex_init_calls++;
    mtx->initialized = true;
    mtx->enter_count = 0;
    mtx->owner = -1;
}

bool mutex_is_initialized(mutex_t *mtx) {
    return mtx->initialized;
}

void mutex_enter_blocking(mutex_t *mtx) {
    pico_fake.mutex_enter_calls++;
    mtx->enter_count++;
}

bool mutex_try_enter(mutex_t *mtx, uint32_t *owner_out) {
    if (pico_fake.mutex_contended) {
        if (owner_out != NULL) {
            *owner_out = 1;
        }
        return false;
    }
    pico_fake.mutex_enter_calls++;
    mtx->enter_count++;
    return true;
}

bool mutex_enter_timeout_us(mutex_t *mtx, uint32_t timeout_us) {
    (void)timeout_us;
    if (pico_fake.mutex_contended) {
        return false;
    }
    pico_fake.mutex_enter_calls++;
    mtx->enter_count++;
    return true;
}

void mutex_exit(mutex_t *mtx) {
    pico_fake.mutex_exit_calls++;
    mtx->enter_count--;
}

/* --------------------------------------------------------------- misc ---- */

bool stdio_init_all(void) {
    pico_fake.stdio_init_all_calls++;
    return true;
}

void sleep_ms(uint32_t ms) {
    pico_fake.sleep_calls++;
    pico_fake.sleep_total_us += (uint64_t)ms * 1000u;
}

void sleep_us(uint64_t us) {
    pico_fake.sleep_calls++;
    pico_fake.sleep_total_us += us;
}

uint32_t time_us_32(void) {
    return (uint32_t)pico_fake.sleep_total_us;
}
