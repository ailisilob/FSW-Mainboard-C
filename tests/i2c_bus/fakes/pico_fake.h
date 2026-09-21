/**
 * @file pico_fake.h
 * @brief Call recorder behind the fake Pico SDK headers.
 *
 * Every faked SDK entry point appends to `pico_fake`. Tests call
 * pico_fake_reset() in setUp(), exercise i2c_bus.c, then assert on what the
 * driver asked the hardware layer to do.
 */

#ifndef PICO_FAKE_H
#define PICO_FAKE_H

#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "pico/stdlib.h"
#include "pico/sync.h"

#define PICO_FAKE_MAX_EVENTS 256

typedef struct {
    uint            gpio;
    gpio_function_t fn;
} pico_fake_gpio_fn_t;

/* One i2c_*_blocking / i2c_*_timeout_us call. */
typedef struct {
    i2c_inst_t     *inst;
    uint8_t         addr;
    const uint8_t  *src;     /* non-NULL for writes */
    uint8_t        *dst;     /* non-NULL for reads */
    size_t          len;
    bool            nostop;
    uint            timeout_us; /* 0 when the *_blocking variant was used */
    bool            timed_variant;
    int             ret;     /* what the fake returned */
} pico_fake_xfer_t;

typedef struct {
    /* --- i2c_init / i2c_deinit / i2c_set_baudrate / i2c_set_slave_mode --- */
    unsigned    i2c_init_calls;
    i2c_inst_t *i2c_init_inst;
    uint        i2c_init_baud;
    uint        i2c_init_return; /* what i2c_init reports as the achieved baud */

    unsigned    i2c_deinit_calls;
    i2c_inst_t *i2c_deinit_inst;

    unsigned    i2c_set_baudrate_calls;
    i2c_inst_t *i2c_set_baudrate_inst;
    uint        i2c_set_baudrate_baud;
    uint        i2c_set_baudrate_return;

    unsigned    i2c_set_slave_mode_calls;
    i2c_inst_t *i2c_set_slave_mode_inst;
    bool        i2c_set_slave_mode_slave;
    uint8_t     i2c_set_slave_mode_addr;

    /* --- gpio --- */
    unsigned            gpio_set_function_calls;
    pico_fake_gpio_fn_t gpio_set_function[PICO_FAKE_MAX_EVENTS];
    unsigned            gpio_pull_up_calls;
    uint                gpio_pull_up_pin[PICO_FAKE_MAX_EVENTS];
    unsigned            gpio_disable_pulls_calls;
    uint                gpio_disable_pulls_pin[PICO_FAKE_MAX_EVENTS];
    unsigned            gpio_put_calls;
    unsigned            gpio_set_dir_calls;
    bool                gpio_level[64]; /* last value driven, for bus-recovery tests */

    /* --- transfers --- */
    unsigned         xfer_calls;
    pico_fake_xfer_t xfer[PICO_FAKE_MAX_EVENTS];
    unsigned         read_calls;
    unsigned         write_calls;

    /* Which 7-bit addresses ACK. Default: none. */
    bool ack[128];
    /* Bytes handed back on a successful read (repeated as needed). */
    uint8_t read_fill;
    /* When set, every transfer returns `forced_ret` instead of consulting ack[]. */
    bool force_ret;
    int  forced_ret;

    /* --- mutex --- */
    unsigned mutex_init_calls;
    unsigned mutex_enter_calls;
    unsigned mutex_exit_calls;
    /* When true, mutex_try_enter / mutex_enter_timeout_us report failure. */
    bool mutex_contended;

    /* --- misc --- */
    unsigned stdio_init_all_calls;
    unsigned sleep_calls;
    uint64_t sleep_total_us;
} pico_fake_t;

extern pico_fake_t pico_fake;

/** Clear all recorded state. Call from setUp(). */
void pico_fake_reset(void);

/** Mark a 7-bit address as present on the bus. */
void pico_fake_set_ack(uint8_t addr, bool acks);

#endif /* PICO_FAKE_H */
