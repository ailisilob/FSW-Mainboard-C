/**
 * @file i2c_bus_fake.h
 * @brief Fake implementation of argus/i2c_bus.h, for testing drivers on top of it.
 *
 * max17205.c's dependency is the i2c_bus API, not the Pico SDK, so that is
 * where these tests cut. Linking the driver against this fake instead of
 * src/board/i2c_bus.c means a max17205 test failure can only be a max17205
 * bug -- i2c_bus.c has its own suite in tests/i2c_bus/.
 *
 * The fake does two things at once:
 *
 *   1. Records every call (i2c_fake.xfer[], plus per-entry-point counters), so
 *      a test can assert on the exact bytes the driver put on the wire.
 *   2. Models a register-map device: 256 16-bit registers per I2C address,
 *      with the usual "write the register pointer, then read" protocol and
 *      little-endian byte order, which is what the MAX17205 speaks. So a test
 *      can just say "register 0x06 reads back 0x8000" and call the driver.
 *
 * Typical use:
 *
 *     void setUp(void) {
 *         i2c_fake_reset();
 *         i2c_fake_set_present(0x36, true);
 *     }
 *
 *     void test_read_soc(void) {
 *         i2c_fake_set_reg(0x36, 0x06, 0x8000);    // 50.0% in 1/256 % units
 *         ...
 *         TEST_ASSERT_EQUAL_INT(0, i2c_fake_find_reg_read(0x36, 0x06) < 0);
 *     }
 *
 * Argument validation mirrors src/board/i2c_bus.c exactly (NULL checks, the
 * 0x08-0x77 address window, bus/device init flags), so a driver that passes a
 * bad argument gets the same status code here as it would on hardware.
 */

#ifndef I2C_BUS_FAKE_H
#define I2C_BUS_FAKE_H

#include "argus/i2c_bus.h"

/** Recorded transfers kept before the log stops growing (counters keep going). */
#define I2C_FAKE_MAX_EVENTS 64
/** Bytes retained per direction per transfer. Longer transfers are truncated
 *  in the log only -- the register model still sees all of them. */
#define I2C_FAKE_MAX_BYTES 16
/** Distinct I2C addresses that can hold a register map at once. The MAX17205
 *  needs two (main + shadow RAM). */
#define I2C_FAKE_MAX_DEVICES 4
/** Registers per device. The MAX17205 register space is 8-bit addressed. */
#define I2C_FAKE_REGS 256

typedef enum {
    i2c_fake_op_write = 0,
    i2c_fake_op_read,
    i2c_fake_op_write_read,
} i2c_fake_op_t;

/** One i2c_read / i2c_write / ic2_write_read call. */
typedef struct {
    i2c_fake_op_t op;
    i2c_bus_t    *bus;
    uint8_t       addr;      /* 7-bit address of the target device */
    uint8_t       reg;       /* register pointer this transfer acted on */
    bool          reg_valid; /* false for a bare read with no pointer ever set */
    uint8_t       w[I2C_FAKE_MAX_BYTES];
    size_t        w_len;     /* length the caller asked for, even if truncated above */
    uint8_t       r[I2C_FAKE_MAX_BYTES];
    size_t        r_len;
    i2c_status_t  ret;       /* what the fake returned */
} i2c_fake_xfer_t;

/** A register-map device sitting at one address. */
typedef struct {
    bool         used;
    uint8_t      addr;
    uint16_t     reg[I2C_FAKE_REGS];
    i2c_status_t reg_status[I2C_FAKE_REGS]; /* i2c_ok unless a failure was injected */
    uint8_t      ptr;                       /* register pointer, as last written */
    bool         ptr_valid;
} i2c_fake_device_t;

typedef struct {
    /* --- bus lifecycle --- */
    unsigned bus_init_calls;
    unsigned bus_deinit_calls;
    unsigned bus_recover_calls;
    unsigned set_baud_calls;
    unsigned set_mode_calls;
    bool     set_mode_slave;   /* argument of the last i2c_set_mode() */
    unsigned scan_bus_calls;

    /* --- locking. depth should be back to 0 after any driver call returns --- */
    unsigned lock_calls;
    unsigned unlock_calls;
    int      lock_depth;
    /** When true, i2c_bus_lock() reports i2c_busy. */
    bool     lock_contended;

    /* --- device bring-up --- */
    unsigned device_init_calls;
    uint8_t  device_init_addr[I2C_FAKE_MAX_EVENTS]; /* in call order */

    /* --- transfers --- */
    unsigned        xfer_calls;    /* total, including any beyond the log */
    unsigned        read_calls;
    unsigned        write_calls;
    unsigned        write_read_calls;
    i2c_fake_xfer_t xfer[I2C_FAKE_MAX_EVENTS];

    /* --- devices --- */
    i2c_fake_device_t dev[I2C_FAKE_MAX_DEVICES];

    /* --- failure injection (see the helpers below) --- */
    i2c_status_t forced;          /* i2c_ok = off; anything else fails every transfer */
    bool         fail_after_armed;
    unsigned     fail_after_remaining;
    i2c_status_t fail_after_status;

    /** Byte returned for a read of a register on a device with no map entry.
     *  Registers all start at 0, so this only shows up past the 256-register
     *  space; it exists to make such a read visibly wrong rather than zero. */
    uint8_t read_fill;
} i2c_fake_t;

extern i2c_fake_t i2c_fake;

/** Clear every recorded call, device and injected failure. Call from setUp(). */
void i2c_fake_reset(void);

/** A bus in the state a successful i2c_bus_init() leaves it in.
 *  Points at static storage that i2c_fake_reset() re-initialises. */
i2c_bus_t *i2c_fake_ready_bus(void);

/** Put a register-map device on the bus at `addr` (or take it off again).
 *  A transfer to an absent address returns i2c_nack_err. */
void i2c_fake_set_present(uint8_t addr, bool present);

/** Set/get one 16-bit register. Setting implies the device is present. */
void     i2c_fake_set_reg(uint8_t addr, uint8_t reg, uint16_t value);
uint16_t i2c_fake_get_reg(uint8_t addr, uint8_t reg);

/** Every transfer returns `st`. Pass i2c_ok to turn it back off. */
void i2c_fake_fail_all(i2c_status_t st);

/** The next `n_ok` transfers succeed; every one after that returns `st`.
 *  Useful for "read_all gives up partway through" cases. */
void i2c_fake_fail_after(unsigned n_ok, i2c_status_t st);

/** Only accesses to this one register fail, with `st`. i2c_ok clears it. */
void i2c_fake_fail_reg(uint8_t addr, uint8_t reg, i2c_status_t st);

/** Most recent recorded transfer, or NULL if there were none. */
const i2c_fake_xfer_t *i2c_fake_last_xfer(void);

/** Index in i2c_fake.xfer[] of the first transfer that read `reg` from `addr`,
 *  or -1. A read spanning several registers matches on any it covered. */
int i2c_fake_find_reg_read(uint8_t addr, uint8_t reg);

/** Index of the first transfer that wrote `value` to `reg` on `addr`, or -1. */
int i2c_fake_find_reg_write(uint8_t addr, uint8_t reg, uint16_t value);

#endif /* I2C_BUS_FAKE_H */
