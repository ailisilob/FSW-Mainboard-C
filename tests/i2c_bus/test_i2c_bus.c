/**
 * @file test_i2c_bus.c
 * @brief Unity unit tests for src/board/i2c_bus.c.
 *
 * These run on the host, not on the RP2350. The Pico SDK calls that i2c_bus.c
 * makes are satisfied by the fakes in fakes/, which record every call so the
 * tests can assert on the driver's side of the hardware boundary.
 *
 * This is a test-first suite: it describes the interface as i2c_bus.h declares
 * it, so a number of these tests fail (or are TEST_IGNOREd) against the current
 * implementation. See README.md for the list and what each one is pointing at.
 *
 * There is no main() and no RUN_TEST list here: Unity's Ruby generator
 * (unity/auto/generate_test_runner.rb) scans this file at build time and emits
 * the runner. Two rules follow from that:
 *
 *   - Test functions must be declared `void test_name(void)`, NOT `static` --
 *     the generator's regex only matches functions starting at `void`, and the
 *     runner links against them from another translation unit. Helpers below
 *     are static on purpose, which is also what keeps them out of the runner.
 *   - Adding a test means writing the function. Nothing else to register.
 */

#include "unity.h"

#include "argus/i2c_bus.h"

#include "pico_fake.h"

#include <string.h>

/* Reserved I2C address ranges: 0x00-0x07 and 0x78-0x7F. */
#define FIRST_VALID_ADDR 0x08
#define LAST_VALID_ADDR  0x77
#define VALID_ADDR_COUNT 112

void setUp(void) {
    pico_fake_reset();
}

void tearDown(void) {
}

/**
 * A bus struct in the state a correct i2c_bus_init() would leave it in.
 *
 * Tests for everything downstream of init build their bus this way so they
 * stay meaningful while i2c_bus_init() itself is still being fixed.
 */
static i2c_bus_t make_ready_bus(i2c_bus_id_t id) {
    i2c_bus_t bus;
    memset(&bus, 0, sizeof(bus));
    bus.i2c = (id == i2c_0) ? i2c0 : i2c1;
    bus.sda = (id == i2c_0) ? I2C_SDA_0 : I2C_SDA_1;
    bus.scl = (id == i2c_0) ? I2C_SCL_0 : I2C_SCL_1;
    bus.baudrate = I2C_baud;
    bus.timeout = I2C_timeout;
    bus.init = true;
    mutex_init(&bus.lock);
    pico_fake_reset(); /* discard the mutex_init bookkeeping from the setup above */
    return bus;
}

/** Index of the recorded gpio_set_function() call for `pin`, or -1. */
static int find_gpio_fn_call(uint pin) {
    for (unsigned i = 0; i < pico_fake.gpio_set_function_calls; i++) {
        if (pico_fake.gpio_set_function[i].gpio == pin) {
            return (int)i;
        }
    }
    return -1;
}

/* ====================================================================== */
/* i2c_bus_init                                                            */
/* ====================================================================== */

void test_bus_init_rejects_null_bus(void) {
    TEST_ASSERT_EQUAL_INT(i2c_arg_err, i2c_bus_init(NULL, i2c_0, I2C_baud, I2C_timeout));
}

void test_bus_init_rejects_out_of_range_id(void) {
    i2c_bus_t bus;
    memset(&bus, 0, sizeof(bus));

    TEST_ASSERT_EQUAL_INT(i2c_arg_err, i2c_bus_init(&bus, bus_count, I2C_baud, I2C_timeout));
    TEST_ASSERT_FALSE(bus.init);
    TEST_ASSERT_EQUAL_UINT(0, pico_fake.i2c_init_calls);
}

void test_bus_init_rejects_zero_baud(void) {
    i2c_bus_t bus;
    memset(&bus, 0, sizeof(bus));

    TEST_ASSERT_EQUAL_INT(i2c_arg_err, i2c_bus_init(&bus, i2c_0, 0, I2C_timeout));
    TEST_ASSERT_EQUAL_UINT(0, pico_fake.i2c_init_calls);
}

void test_bus_init_i2c0_selects_hw_block_and_pins(void) {
    i2c_bus_t bus;
    memset(&bus, 0, sizeof(bus));

    TEST_ASSERT_EQUAL_INT(i2c_ok, i2c_bus_init(&bus, i2c_0, I2C_baud, I2C_timeout));
    TEST_ASSERT_EQUAL_PTR(i2c0, bus.i2c);
    TEST_ASSERT_EQUAL_UINT(I2C_SDA_0, bus.sda);
    TEST_ASSERT_EQUAL_UINT(I2C_SCL_0, bus.scl);
}

void test_bus_init_i2c1_selects_hw_block_and_pins(void) {
    i2c_bus_t bus;
    memset(&bus, 0, sizeof(bus));

    TEST_ASSERT_EQUAL_INT(i2c_ok, i2c_bus_init(&bus, i2c_1, I2C_baud, I2C_timeout));
    TEST_ASSERT_EQUAL_PTR(i2c1, bus.i2c);
    TEST_ASSERT_EQUAL_UINT(I2C_SDA_1, bus.sda);
    TEST_ASSERT_EQUAL_UINT(I2C_SCL_1, bus.scl);
}

void test_bus_init_passes_the_instance_to_the_sdk(void) {
    i2c_bus_t bus;
    memset(&bus, 0, sizeof(bus));

    (void)i2c_bus_init(&bus, i2c_1, I2C_baud, I2C_timeout);

    TEST_ASSERT_EQUAL_UINT(1, pico_fake.i2c_init_calls);
    TEST_ASSERT_EQUAL_PTR_MESSAGE(i2c1, pico_fake.i2c_init_inst,
                              "i2c_init() must get the i2c_inst_t*, not the address of the field "
                              "holding it");
    TEST_ASSERT_EQUAL_UINT(I2C_baud, pico_fake.i2c_init_baud);
}

void test_bus_init_records_achieved_baudrate(void) {
    i2c_bus_t bus;
    memset(&bus, 0, sizeof(bus));

    /* The SDK reports the baud it could actually reach, which may differ. */
    pico_fake.i2c_init_return = 373000;

    (void)i2c_bus_init(&bus, i2c_0, I2C_baud, I2C_timeout);

    TEST_ASSERT_EQUAL_UINT(373000, bus.baudrate);
}

void test_bus_init_stores_timeout(void) {
    i2c_bus_t bus;
    memset(&bus, 0, sizeof(bus));

    (void)i2c_bus_init(&bus, i2c_0, I2C_baud, 12345);

    TEST_ASSERT_EQUAL_UINT(12345, bus.timeout);
}

void test_bus_init_muxes_pins_to_i2c_with_pullups(void) {
    i2c_bus_t bus;
    memset(&bus, 0, sizeof(bus));

    (void)i2c_bus_init(&bus, i2c_0, I2C_baud, I2C_timeout);

    int sda = find_gpio_fn_call(I2C_SDA_0);
    int scl = find_gpio_fn_call(I2C_SCL_0);
    TEST_ASSERT_MESSAGE(sda >= 0, "SDA was never muxed");
    TEST_ASSERT_MESSAGE(scl >= 0, "SCL was never muxed");
    TEST_ASSERT_EQUAL_INT(GPIO_FUNC_I2C, pico_fake.gpio_set_function[sda].fn);
    TEST_ASSERT_EQUAL_INT(GPIO_FUNC_I2C, pico_fake.gpio_set_function[scl].fn);

    TEST_ASSERT_EQUAL_UINT(2, pico_fake.gpio_pull_up_calls);
}

void test_bus_init_marks_bus_initialized(void) {
    i2c_bus_t bus;
    memset(&bus, 0, sizeof(bus));

    (void)i2c_bus_init(&bus, i2c_0, I2C_baud, I2C_timeout);

    TEST_ASSERT_TRUE(bus.init);
}

void test_bus_init_creates_the_lock_once(void) {
    i2c_bus_t bus;
    memset(&bus, 0, sizeof(bus));

    (void)i2c_bus_init(&bus, i2c_0, I2C_baud, I2C_timeout);
    TEST_ASSERT_EQUAL_UINT(1, pico_fake.mutex_init_calls);

    /* Re-initialising the bus must not stomp a lock somebody may be holding. */
    (void)i2c_bus_init(&bus, i2c_0, I2C_baud, I2C_timeout);
    TEST_ASSERT_EQUAL_UINT(1, pico_fake.mutex_init_calls);
}

/* ====================================================================== */
/* i2c_bus_deinit                                                          */
/* ====================================================================== */

void test_bus_deinit_null_bus_is_a_noop(void) {
    i2c_bus_deinit(NULL); /* must not dereference */
    TEST_ASSERT_EQUAL_UINT(0, pico_fake.i2c_deinit_calls);
}

void test_bus_deinit_uninitialized_bus_is_a_noop(void) {
    i2c_bus_t bus;
    memset(&bus, 0, sizeof(bus));
    bus.init = false;

    i2c_bus_deinit(&bus);

    TEST_ASSERT_EQUAL_UINT(0, pico_fake.i2c_deinit_calls);
    TEST_ASSERT_EQUAL_UINT(0, pico_fake.gpio_set_function_calls);
}

void test_bus_deinit_releases_hardware(void) {
    i2c_bus_t bus = make_ready_bus(i2c_0);

    i2c_bus_deinit(&bus);

    TEST_ASSERT_EQUAL_UINT(1, pico_fake.i2c_deinit_calls);
    TEST_ASSERT_EQUAL_PTR(i2c0, pico_fake.i2c_deinit_inst);

    int sda = find_gpio_fn_call(I2C_SDA_0);
    int scl = find_gpio_fn_call(I2C_SCL_0);
    TEST_ASSERT_MESSAGE(sda >= 0 && scl >= 0, "pins were not returned to SIO");
    TEST_ASSERT_EQUAL_INT(GPIO_FUNC_SIO, pico_fake.gpio_set_function[sda].fn);
    TEST_ASSERT_EQUAL_INT(GPIO_FUNC_SIO, pico_fake.gpio_set_function[scl].fn);
}

void test_bus_deinit_clears_init_flag(void) {
    i2c_bus_t bus = make_ready_bus(i2c_1);

    i2c_bus_deinit(&bus);

    TEST_ASSERT_FALSE(bus.init);
}

void test_bus_deinit_is_idempotent(void) {
    i2c_bus_t bus = make_ready_bus(i2c_0);

    i2c_bus_deinit(&bus);
    i2c_bus_deinit(&bus);

    TEST_ASSERT_EQUAL_UINT(1, pico_fake.i2c_deinit_calls);
}

/* ====================================================================== */
/* i2c_device_init                                                         */
/* ====================================================================== */

void test_device_init_rejects_null_device(void) {
    i2c_bus_t bus = make_ready_bus(i2c_0);
    TEST_ASSERT_EQUAL_INT(i2c_arg_err, i2c_device_init(NULL, &bus, 0x36));
}

void test_device_init_rejects_null_bus(void) {
    i2c_device_t dev;
    memset(&dev, 0, sizeof(dev));
    TEST_ASSERT_EQUAL_INT(i2c_arg_err, i2c_device_init(&dev, NULL, 0x36));
}

void test_device_init_rejects_reserved_low_addresses(void) {
    i2c_bus_t bus = make_ready_bus(i2c_0);

    for (uint8_t addr = 0x00; addr < FIRST_VALID_ADDR; addr++) {
        i2c_device_t dev;
        memset(&dev, 0, sizeof(dev));
        char msg[48];
        snprintf(msg, sizeof(msg), "addr 0x%02x is reserved", addr);
        TEST_ASSERT_EQUAL_INT_MESSAGE(i2c_arg_err, i2c_device_init(&dev, &bus, addr), msg);
    }
}

void test_device_init_rejects_reserved_high_addresses(void) {
    i2c_bus_t bus = make_ready_bus(i2c_0);

    for (uint8_t addr = 0x78; addr < 0x80; addr++) {
        i2c_device_t dev;
        memset(&dev, 0, sizeof(dev));
        char msg[48];
        snprintf(msg, sizeof(msg), "addr 0x%02x is reserved", addr);
        TEST_ASSERT_EQUAL_INT_MESSAGE(i2c_arg_err, i2c_device_init(&dev, &bus, addr), msg);
    }
}

void test_device_init_rejects_addresses_above_7_bits(void) {
    i2c_bus_t bus = make_ready_bus(i2c_0);
    i2c_device_t dev;
    memset(&dev, 0, sizeof(dev));

    TEST_ASSERT_EQUAL_INT(i2c_arg_err, i2c_device_init(&dev, &bus, 0x80));
    TEST_ASSERT_EQUAL_INT(i2c_arg_err, i2c_device_init(&dev, &bus, 0xFF));
}

void test_device_init_accepts_the_whole_valid_range(void) {
    i2c_bus_t bus = make_ready_bus(i2c_0);

    for (uint8_t addr = FIRST_VALID_ADDR; addr <= LAST_VALID_ADDR; addr++) {
        i2c_device_t dev;
        memset(&dev, 0, sizeof(dev));
        char msg[48];
        snprintf(msg, sizeof(msg), "addr 0x%02x should be accepted", addr);
        TEST_ASSERT_EQUAL_INT_MESSAGE(i2c_ok, i2c_device_init(&dev, &bus, addr), msg);
    }
}

void test_device_init_binds_bus_and_address(void) {
    i2c_bus_t bus = make_ready_bus(i2c_1);
    i2c_device_t dev;
    memset(&dev, 0, sizeof(dev));

    /* 0x36 is the MAX17205 fuel gauge address used elsewhere in this project. */
    TEST_ASSERT_EQUAL_INT(i2c_ok, i2c_device_init(&dev, &bus, 0x36));
    TEST_ASSERT_EQUAL_PTR(&bus, dev.bus);
    TEST_ASSERT_EQUAL_UINT8(0x36, dev.addr);
}

void test_device_init_rejects_uninitialized_bus(void) {
    i2c_bus_t bus;
    memset(&bus, 0, sizeof(bus));
    bus.init = false;

    i2c_device_t dev;
    memset(&dev, 0, sizeof(dev));

    TEST_ASSERT_EQUAL_INT(i2c_bus_not_init, i2c_device_init(&dev, &bus, 0x36));
}

/* ====================================================================== */
/* i2c_read                                                                */
/* ====================================================================== */

/** A device bound to a ready bus, for the transfer tests. */
static i2c_device_t make_device(i2c_bus_t *bus, uint8_t addr) {
    i2c_device_t dev;
    memset(&dev, 0, sizeof(dev));
    dev.bus = bus;
    dev.addr = addr;
    return dev;
}

void test_read_rejects_null_arguments(void) {
    i2c_bus_t bus = make_ready_bus(i2c_0);
    i2c_device_t dev = make_device(&bus, 0x36);
    uint8_t buf[4];

    TEST_ASSERT_EQUAL_INT(i2c_arg_err, i2c_read(NULL, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_INT(i2c_arg_err, i2c_read(&dev, NULL, sizeof(buf)));
    TEST_ASSERT_EQUAL_INT(i2c_arg_err, i2c_read(&dev, buf, 0));
    TEST_ASSERT_EQUAL_UINT(0, pico_fake.xfer_calls);
}

void test_read_issues_a_transfer_to_the_device(void) {
    TEST_IGNORE_MESSAGE("i2c_read() has no success path yet");

    i2c_bus_t bus = make_ready_bus(i2c_0);
    i2c_device_t dev = make_device(&bus, 0x36);
    uint8_t buf[4] = {0};

    pico_fake_set_ack(0x36, true);

    TEST_ASSERT_EQUAL_INT(i2c_ok, i2c_read(&dev, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_UINT(1, pico_fake.read_calls);
    TEST_ASSERT_EQUAL_PTR(i2c0, pico_fake.xfer[0].inst);
    TEST_ASSERT_EQUAL_UINT8(0x36, pico_fake.xfer[0].addr);
    TEST_ASSERT_EQUAL_PTR(buf, pico_fake.xfer[0].dst);
    TEST_ASSERT_EQUAL_size_t(sizeof(buf), pico_fake.xfer[0].len);
    TEST_ASSERT_FALSE_MESSAGE(pico_fake.xfer[0].nostop, "a standalone read must release the bus");
}

void test_read_honours_the_bus_timeout(void) {
    TEST_IGNORE_MESSAGE("i2c_read() does not use bus->timeout yet");

    i2c_bus_t bus = make_ready_bus(i2c_0);
    i2c_device_t dev = make_device(&bus, 0x36);
    uint8_t buf[2] = {0};

    pico_fake_set_ack(0x36, true);
    (void)i2c_read(&dev, buf, sizeof(buf));

    TEST_ASSERT_TRUE_MESSAGE(pico_fake.xfer[0].timed_variant,
                             "bus->timeout is stored but never applied to a transfer");
    TEST_ASSERT_EQUAL_UINT(I2C_timeout, pico_fake.xfer[0].timeout_us);
}

void test_read_reports_nack(void) {
    TEST_IGNORE_MESSAGE("i2c_read() has no error mapping yet");

    i2c_bus_t bus = make_ready_bus(i2c_0);
    i2c_device_t dev = make_device(&bus, 0x36);
    uint8_t buf[2] = {0};

    pico_fake_set_ack(0x36, false); /* nobody home */

    TEST_ASSERT_EQUAL_INT(i2c_nack_err, i2c_read(&dev, buf, sizeof(buf)));
}

void test_read_reports_timeout(void) {
    TEST_IGNORE_MESSAGE("i2c_read() has no error mapping yet");

    i2c_bus_t bus = make_ready_bus(i2c_0);
    i2c_device_t dev = make_device(&bus, 0x36);
    uint8_t buf[2] = {0};

    pico_fake.force_ret = true;
    pico_fake.forced_ret = PICO_ERROR_TIMEOUT;

    TEST_ASSERT_EQUAL_INT(i2c_timeout, i2c_read(&dev, buf, sizeof(buf)));
}

void test_read_rejects_uninitialized_bus(void) {
    TEST_IGNORE_MESSAGE("i2c_read() does not check bus->init yet");

    i2c_bus_t bus = make_ready_bus(i2c_0);
    bus.init = false;
    i2c_device_t dev = make_device(&bus, 0x36);
    uint8_t buf[2] = {0};

    TEST_ASSERT_EQUAL_INT(i2c_bus_not_init, i2c_read(&dev, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_UINT(0, pico_fake.xfer_calls);
}

void test_read_rejects_device_with_no_bus(void) {
    TEST_IGNORE_MESSAGE("i2c_read() does not check dev->bus yet");

    i2c_device_t dev = make_device(NULL, 0x36);
    uint8_t buf[2] = {0};

    TEST_ASSERT_EQUAL_INT(i2c_dev_not_init, i2c_read(&dev, buf, sizeof(buf)));
}

/* ====================================================================== */
/* i2c_write                                                               */
/* ====================================================================== */

void test_write_rejects_null_arguments(void) {
    i2c_bus_t bus = make_ready_bus(i2c_0);
    i2c_device_t dev = make_device(&bus, 0x36);
    const uint8_t payload[2] = {0x01, 0x02};

    TEST_ASSERT_EQUAL_INT(i2c_arg_err, i2c_write(NULL, payload, sizeof(payload)));
    TEST_ASSERT_EQUAL_INT(i2c_arg_err, i2c_write(&dev, NULL, sizeof(payload)));
    TEST_ASSERT_EQUAL_INT(i2c_arg_err, i2c_write(&dev, payload, 0));
    TEST_ASSERT_EQUAL_UINT(0, pico_fake.xfer_calls);
}

void test_write_issues_a_transfer_to_the_device(void) {
    TEST_IGNORE_MESSAGE("i2c_write() has no success path yet");

    i2c_bus_t bus = make_ready_bus(i2c_1);
    i2c_device_t dev = make_device(&bus, 0x36);
    const uint8_t payload[3] = {0x0A, 0x0B, 0x0C};

    pico_fake_set_ack(0x36, true);

    TEST_ASSERT_EQUAL_INT(i2c_ok, i2c_write(&dev, payload, sizeof(payload)));
    TEST_ASSERT_EQUAL_UINT(1, pico_fake.write_calls);
    TEST_ASSERT_EQUAL_PTR(i2c1, pico_fake.xfer[0].inst);
    TEST_ASSERT_EQUAL_UINT8(0x36, pico_fake.xfer[0].addr);
    TEST_ASSERT_EQUAL_PTR(payload, pico_fake.xfer[0].src);
    TEST_ASSERT_EQUAL_size_t(sizeof(payload), pico_fake.xfer[0].len);
    TEST_ASSERT_FALSE(pico_fake.xfer[0].nostop);
}

void test_write_reports_nack(void) {
    TEST_IGNORE_MESSAGE("i2c_write() has no error mapping yet");

    i2c_bus_t bus = make_ready_bus(i2c_0);
    i2c_device_t dev = make_device(&bus, 0x36);
    const uint8_t payload[1] = {0xFF};

    pico_fake_set_ack(0x36, false);

    TEST_ASSERT_EQUAL_INT(i2c_nack_err, i2c_write(&dev, payload, sizeof(payload)));
}

void test_write_rejects_uninitialized_bus(void) {
    TEST_IGNORE_MESSAGE("i2c_write() does not check bus->init yet");

    i2c_bus_t bus = make_ready_bus(i2c_0);
    bus.init = false;
    i2c_device_t dev = make_device(&bus, 0x36);
    const uint8_t payload[1] = {0xFF};

    TEST_ASSERT_EQUAL_INT(i2c_bus_not_init, i2c_write(&dev, payload, sizeof(payload)));
    TEST_ASSERT_EQUAL_UINT(0, pico_fake.xfer_calls);
}

/* ====================================================================== */
/* ic2_write_read  (note: the declared name has the typo, not this file)   */
/* ====================================================================== */

void test_write_read_rejects_null_arguments(void) {
    TEST_IGNORE_MESSAGE("ic2_write_read() is unimplemented");

    i2c_bus_t bus = make_ready_bus(i2c_0);
    i2c_device_t dev = make_device(&bus, 0x36);
    const uint8_t reg[1] = {0x02};
    uint8_t buf[2];

    TEST_ASSERT_EQUAL_INT(i2c_arg_err, ic2_write_read(NULL, reg, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_INT(i2c_arg_err, ic2_write_read(&dev, NULL, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_INT(i2c_arg_err, ic2_write_read(&dev, reg, NULL, sizeof(buf)));
    TEST_ASSERT_EQUAL_INT(i2c_arg_err, ic2_write_read(&dev, reg, buf, 0));
}

void test_write_read_does_a_repeated_start(void) {
    TEST_IGNORE_MESSAGE("ic2_write_read() is unimplemented");

    i2c_bus_t bus = make_ready_bus(i2c_0);
    i2c_device_t dev = make_device(&bus, 0x36);
    const uint8_t reg[1] = {0x02};
    uint8_t buf[2] = {0};

    pico_fake_set_ack(0x36, true);

    TEST_ASSERT_EQUAL_INT(i2c_ok, ic2_write_read(&dev, reg, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_UINT(2, pico_fake.xfer_calls);

    /* Write the register pointer, hold the bus, then read the value back. */
    TEST_ASSERT_EQUAL_PTR(reg, pico_fake.xfer[0].src);
    TEST_ASSERT_TRUE_MESSAGE(pico_fake.xfer[0].nostop,
                             "the address write must not issue a STOP");
    TEST_ASSERT_EQUAL_PTR(buf, pico_fake.xfer[1].dst);
    TEST_ASSERT_FALSE(pico_fake.xfer[1].nostop);
}

/* ====================================================================== */
/* Locking                                                                 */
/* ====================================================================== */

void test_bus_lock_takes_the_mutex(void) {
    TEST_IGNORE_MESSAGE("i2c_bus_lock() is unimplemented");

    i2c_bus_t bus = make_ready_bus(i2c_0);

    TEST_ASSERT_EQUAL_INT(i2c_ok, i2c_bus_lock(&bus));
    TEST_ASSERT_EQUAL_UINT(1, pico_fake.mutex_enter_calls);
}

void test_bus_lock_rejects_null_and_uninitialized(void) {
    TEST_IGNORE_MESSAGE("i2c_bus_lock() is unimplemented");

    i2c_bus_t bus = make_ready_bus(i2c_0);
    bus.init = false;

    TEST_ASSERT_EQUAL_INT(i2c_arg_err, i2c_bus_lock(NULL));
    TEST_ASSERT_EQUAL_INT(i2c_bus_not_init, i2c_bus_lock(&bus));
}

void test_bus_lock_reports_busy_when_contended(void) {
    TEST_IGNORE_MESSAGE("i2c_bus_lock() is unimplemented");

    i2c_bus_t bus = make_ready_bus(i2c_0);
    pico_fake.mutex_contended = true;

    TEST_ASSERT_EQUAL_INT(i2c_busy, i2c_bus_lock(&bus));
}

void test_bus_unlock_releases_the_mutex(void) {
    TEST_IGNORE_MESSAGE("i2c_bus_unlock() is unimplemented");

    i2c_bus_t bus = make_ready_bus(i2c_0);
    (void)i2c_bus_lock(&bus);

    i2c_bus_unlock(&bus);

    TEST_ASSERT_EQUAL_UINT(1, pico_fake.mutex_exit_calls);
    TEST_ASSERT_EQUAL_INT(0, bus.lock.enter_count);
}

void test_bus_unlock_null_is_a_noop(void) {
    TEST_IGNORE_MESSAGE("i2c_bus_unlock() is unimplemented");

    i2c_bus_unlock(NULL);
    TEST_ASSERT_EQUAL_UINT(0, pico_fake.mutex_exit_calls);
}

/* ====================================================================== */
/* Bus settings                                                            */
/* ====================================================================== */

void test_set_baud_pushes_to_hardware(void) {
    TEST_IGNORE_MESSAGE(
        "i2c_set_baud() is unimplemented, and its signature takes no baud rate to set");

    i2c_bus_t bus = make_ready_bus(i2c_0);
    bus.baudrate = 100000;

    TEST_ASSERT_EQUAL_INT(i2c_ok, i2c_set_baud(&bus));
    TEST_ASSERT_EQUAL_UINT(1, pico_fake.i2c_set_baudrate_calls);
    TEST_ASSERT_EQUAL_PTR(i2c0, pico_fake.i2c_set_baudrate_inst);
    TEST_ASSERT_EQUAL_UINT(100000, pico_fake.i2c_set_baudrate_baud);
}

void test_set_baud_rejects_null_and_uninitialized(void) {
    TEST_IGNORE_MESSAGE("i2c_set_baud() is unimplemented");

    i2c_bus_t bus = make_ready_bus(i2c_0);
    bus.init = false;

    TEST_ASSERT_EQUAL_INT(i2c_arg_err, i2c_set_baud(NULL));
    TEST_ASSERT_EQUAL_INT(i2c_bus_not_init, i2c_set_baud(&bus));
}

void test_set_mode_rejects_null_and_uninitialized(void) {
    TEST_IGNORE_MESSAGE("i2c_set_mode() is unimplemented and its contract is undefined");

    i2c_bus_t bus = make_ready_bus(i2c_0);
    bus.init = false;

    TEST_ASSERT_EQUAL_INT(i2c_arg_err, i2c_set_mode(NULL));
    TEST_ASSERT_EQUAL_INT(i2c_bus_not_init, i2c_set_mode(&bus));
}

/* ====================================================================== */
/* i2c_bus_recover                                                         */
/* ====================================================================== */

void test_bus_recover_rejects_null_and_uninitialized(void) {
    TEST_IGNORE_MESSAGE("i2c_bus_recover() is unimplemented");

    i2c_bus_t bus = make_ready_bus(i2c_0);
    bus.init = false;

    TEST_ASSERT_EQUAL_INT(i2c_arg_err, i2c_bus_recover(NULL));
    TEST_ASSERT_EQUAL_INT(i2c_bus_not_init, i2c_bus_recover(&bus));
}

void test_bus_recover_clocks_the_bus_free(void) {
    TEST_IGNORE_MESSAGE("i2c_bus_recover() is unimplemented");

    i2c_bus_t bus = make_ready_bus(i2c_0);

    TEST_ASSERT_EQUAL_INT(i2c_ok, i2c_bus_recover(&bus));

    /* Standard recovery: take the pins to SIO, pulse SCL up to 9 times to let a
     * stuck slave finish its byte, then hand the pins back to the I2C block. */
    TEST_ASSERT_GREATER_OR_EQUAL_UINT(9, pico_fake.gpio_put_calls);
    int scl = find_gpio_fn_call(I2C_SCL_0);
    TEST_ASSERT_MESSAGE(scl >= 0, "SCL was never re-muxed");
    TEST_ASSERT_EQUAL_INT_MESSAGE(
        GPIO_FUNC_I2C,
        pico_fake.gpio_set_function[pico_fake.gpio_set_function_calls - 1].fn,
        "pins must end up back under the I2C block");
}

/* ====================================================================== */
/* scan_bus                                                                */
/* ====================================================================== */

void test_scan_bus_rejects_null_bus(void) {
    TEST_ASSERT_EQUAL_INT(i2c_arg_err, scan_bus(NULL));
}

void test_scan_bus_rejects_uninitialized_bus(void) {
    i2c_bus_t bus;
    memset(&bus, 0, sizeof(bus));
    bus.init = false;

    TEST_ASSERT_EQUAL_INT(i2c_bus_not_init, scan_bus(&bus));
    TEST_ASSERT_EQUAL_UINT(0, pico_fake.xfer_calls);
}

void test_scan_bus_skips_reserved_addresses(void) {
    i2c_bus_t bus = make_ready_bus(i2c_0);

    TEST_ASSERT_EQUAL_INT(i2c_ok, scan_bus(&bus));

    TEST_ASSERT_EQUAL_UINT_MESSAGE(VALID_ADDR_COUNT, pico_fake.read_calls,
                                   "only the 112 non-reserved addresses should be probed");
    for (unsigned i = 0; i < pico_fake.xfer_calls; i++) {
        uint8_t addr = pico_fake.xfer[i].addr;
        char msg[56];
        snprintf(msg, sizeof(msg), "reserved addr 0x%02x was probed", addr);
        TEST_ASSERT_MESSAGE(addr >= FIRST_VALID_ADDR && addr <= LAST_VALID_ADDR, msg);
    }
}

void test_scan_bus_probes_one_byte_per_address(void) {
    i2c_bus_t bus = make_ready_bus(i2c_0);

    (void)scan_bus(&bus);

    for (unsigned i = 0; i < pico_fake.xfer_calls; i++) {
        TEST_ASSERT_EQUAL_size_t(1, pico_fake.xfer[i].len);
        TEST_ASSERT_NOT_NULL(pico_fake.xfer[i].dst);
    }
}

void test_scan_bus_probes_the_bus_it_was_given(void) {
    i2c_bus_t bus = make_ready_bus(i2c_1);

    (void)scan_bus(&bus);

    TEST_ASSERT_GREATER_THAN_UINT(0, pico_fake.xfer_calls);
    for (unsigned i = 0; i < pico_fake.xfer_calls; i++) {
        TEST_ASSERT_EQUAL_PTR_MESSAGE(i2c1, pico_fake.xfer[i].inst,
                                      "scan_bus() must use bus->i2c, not i2c_default");
    }
}

