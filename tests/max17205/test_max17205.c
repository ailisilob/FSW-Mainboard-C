/**
 * @file test_max17205.c
 * @brief Unity unit tests for src/drivers/max17205.c.
 *
 * These run on the host, not on the RP2350. The driver is linked against the
 * fake i2c_bus in fakes/, which models the gauge as a register map, so each
 * test stages a register value, calls the driver, and checks what came back.
 *
 * One test per function in max17205.h. Each read test also checks that the
 * function rejects NULL arguments.
 *
 * This is a test-first suite: it describes the interface as max17205.h
 * declares it, so it fails against the current stub implementation.
 *
 */

#include "unity.h"

#include "argus/i2c_bus.h"
#include "argus/drivers/max17205.h"

#include "i2c_bus_fake.h"

#include <string.h>

/* The gauge answers at two 7-bit addresses: the main register block, and the
 * shadow RAM holding the thermistor and die temperatures. (0x6C and 0x16 in
 * the datasheet's 8-bit notation.) */
#define GAUGE_MAIN   0x36
#define GAUGE_SHADOW 0x0B

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

/* 0x000F to the Command register is the datasheet's full reset. */
#define MAX17205_CMD_FULL_RESET 0x000F

/* Raw register value -> the units max17205_readings_t stores. */
static uint32_t expect_soc(uint16_t raw) {
    return (uint32_t)raw / 256;
}

static uint32_t expect_capacity(uint16_t raw) {
    return (uint32_t)raw * 0.5;
}

static int32_t expect_current(int16_t raw) {
    return (int32_t)raw * 0.0015625 / 0.01;
}

static uint32_t expect_voltage(uint16_t raw) {
    return (uint32_t)raw * 1.25;
}

static uint32_t expect_midvoltage(uint16_t raw) {
    return (uint32_t)raw * 0.078125;
}

/* ========================================================================= */
/* Fixture                                                                   */
/* ========================================================================= */

void setUp(void) {
    i2c_fake_reset();
    /* Both halves of the gauge ACK by default. */
    i2c_fake_set_present(GAUGE_MAIN, true);
    i2c_fake_set_present(GAUGE_SHADOW, true);
}

void tearDown(void) {
    /* Nothing may leave the bus locked, whatever path the driver took. */
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, i2c_fake.lock_depth,
                                  "driver returned while still holding the bus lock");
}

/**
 * A max17205_t in the state a correct max17205_init() would leave it in.
 *
 * The read and reset tests build their device this way so they stay
 * meaningful while max17205_init() itself is still a stub.
 */
static max17205_t make_ready_dev(void) {
    max17205_t dev;
    memset(&dev, 0, sizeof(dev));
    dev.main.bus = i2c_fake_ready_bus();
    dev.main.addr = GAUGE_MAIN;
    dev.shadow.bus = i2c_fake_ready_bus();
    dev.shadow.addr = GAUGE_SHADOW;
    dev.init = true;
    return dev;
}

/** A bus that has been brought up. */
static i2c_bus_t *make_ready_bus(void) {
    return i2c_fake_ready_bus();
}

/** 
 * 
 * max17205 initialization tests 
 * 
 * */
void test_max17205_init_rejects_null_dev(void) {
    TEST_ASSERT_EQUAL_INT(max17205_arg_err, max17205_init(NULL, make_ready_bus()));
}

void test_max17205_init_rejects_null_bus(void) {
    max17205_t dev;
    memset(&dev, 0, sizeof(dev));

    TEST_ASSERT_EQUAL_INT(max17205_arg_err, max17205_init(&dev, NULL));
    TEST_ASSERT_FALSE_MESSAGE(dev.init, "a rejected init must not mark the device ready");
}

void test_max17205_deinit_rejects_null(void) {
    TEST_ASSERT_EQUAL_INT(max17205_arg_err, max17205_deinit(NULL));
}

/* max17205 read value tests */
void test_max17205_read_soc(void) {
    max17205_t dev = make_ready_dev();
    max17205_readings_t val;
    memset(&val, 0, sizeof(val));

    TEST_ASSERT_EQUAL_INT(max17205_arg_err, max17205_read_soc(NULL, &val));
    TEST_ASSERT_EQUAL_INT(max17205_arg_err, max17205_read_soc(&dev, NULL));

    /* RepSOC: 1/256 % per LSB. 0x3200 = 50 %. */
    i2c_fake_set_reg(GAUGE_MAIN, MAX17205_REPSOC_ADDR, 0x3200);
    TEST_ASSERT_EQUAL_INT(max17205_ok, max17205_read_soc(&dev, &val));
    TEST_ASSERT_EQUAL_UINT32(expect_soc(0x3200), val.soc);
}

void test_max17205_read_capacity(void) {
    max17205_t dev = make_ready_dev();
    max17205_readings_t val;
    memset(&val, 0, sizeof(val));

    TEST_ASSERT_EQUAL_INT(max17205_arg_err, max17205_read_capacity(NULL, &val));
    TEST_ASSERT_EQUAL_INT(max17205_arg_err, max17205_read_capacity(&dev, NULL));

    /* FullCapRep: 5.0 uVh/Rsense per LSB. At 10 mOhm, 3000 LSB = 1500 mAh. */
    i2c_fake_set_reg(GAUGE_MAIN, MAX17205_CAPACITY_ADDR, 3000);
    TEST_ASSERT_EQUAL_INT(max17205_ok, max17205_read_capacity(&dev, &val));
    TEST_ASSERT_EQUAL_UINT32(expect_capacity(3000), val.capacity);
}

void test_max17205_read_current(void) {
    max17205_t dev = make_ready_dev();
    max17205_readings_t val;
    memset(&val, 0, sizeof(val));

    TEST_ASSERT_EQUAL_INT(max17205_arg_err, max17205_read_current(NULL, &val));
    TEST_ASSERT_EQUAL_INT(max17205_arg_err, max17205_read_current(&dev, NULL));

    /* Current: 1.5625 uV/Rsense per LSB. At 10 mOhm, 256 LSB = 40 mA. */
    i2c_fake_set_reg(GAUGE_MAIN, MAX17205_CURRENT_ADDR, 256);
    TEST_ASSERT_EQUAL_INT(max17205_ok, max17205_read_current(&dev, &val));
    TEST_ASSERT_EQUAL_INT32(expect_current(256), val.current);
}

void test_max17205_read_voltage(void) {
    max17205_t dev = make_ready_dev();
    max17205_readings_t val;
    memset(&val, 0, sizeof(val));

    TEST_ASSERT_EQUAL_INT(max17205_arg_err, max17205_read_voltage(NULL, &val));
    TEST_ASSERT_EQUAL_INT(max17205_arg_err, max17205_read_voltage(&dev, NULL));

    /* Batt: 1.25 mV per LSB. 6400 LSB = 8000 mV, a full 2S pack. */
    i2c_fake_set_reg(GAUGE_MAIN, MAX17205_VBAT_ADDR, 6400);
    TEST_ASSERT_EQUAL_INT(max17205_ok, max17205_read_voltage(&dev, &val));
    TEST_ASSERT_EQUAL_UINT32(expect_voltage(6400), val.voltage);
}

void test_max17205_read_midvoltage(void) {
    max17205_t dev = make_ready_dev();
    max17205_readings_t val;
    memset(&val, 0, sizeof(val));

    TEST_ASSERT_EQUAL_INT(max17205_arg_err, max17205_read_midvoltage(NULL, &val));
    TEST_ASSERT_EQUAL_INT(max17205_arg_err, max17205_read_midvoltage(&dev, NULL));

    /* VCell: 78.125 uV per LSB. 40960 LSB = 3200 mV. */
    i2c_fake_set_reg(GAUGE_MAIN, MAX17205_VCELL_ADDR, 40960);
    TEST_ASSERT_EQUAL_INT(max17205_ok, max17205_read_midvoltage(&dev, &val));
    TEST_ASSERT_EQUAL_UINT32(expect_midvoltage(40960), val.midvoltage);
}

void test_max17205_read_cycles(void) {
    max17205_t dev = make_ready_dev();
    max17205_readings_t val;
    memset(&val, 0, sizeof(val));

    TEST_ASSERT_EQUAL_INT(max17205_arg_err, max17205_read_cycles(NULL, &val));
    TEST_ASSERT_EQUAL_INT(max17205_arg_err, max17205_read_cycles(&dev, NULL));

    i2c_fake_set_reg(GAUGE_MAIN, MAX17205_AVCELL_ADDR, 100);
    TEST_ASSERT_EQUAL_INT(max17205_ok, max17205_read_cycles(&dev, &val));
    TEST_ASSERT_EQUAL_UINT16(100, val.cycles);
}

void test_max17205_read_tte(void) {
    max17205_t dev = make_ready_dev();
    max17205_readings_t val;
    memset(&val, 0, sizeof(val));

    TEST_ASSERT_EQUAL_INT(max17205_arg_err, max17205_read_tte(NULL, &val));
    TEST_ASSERT_EQUAL_INT(max17205_arg_err, max17205_read_tte(&dev, NULL));

    i2c_fake_set_reg(GAUGE_MAIN, MAX17205_TTE_ADDR, 0x0400);
    TEST_ASSERT_EQUAL_INT(max17205_ok, max17205_read_tte(&dev, &val));
    TEST_ASSERT_EQUAL_UINT16(0x0400, val.tte);
}

void test_max17205_read_ttf(void) {
    max17205_t dev = make_ready_dev();
    max17205_readings_t val;
    memset(&val, 0, sizeof(val));

    TEST_ASSERT_EQUAL_INT(max17205_arg_err, max17205_read_ttf(NULL, &val));
    TEST_ASSERT_EQUAL_INT(max17205_arg_err, max17205_read_ttf(&dev, NULL));

    i2c_fake_set_reg(GAUGE_MAIN, MAX17205_TTF_ADDR, 0x0800);
    TEST_ASSERT_EQUAL_INT(max17205_ok, max17205_read_ttf(&dev, &val));
    TEST_ASSERT_EQUAL_UINT16(0x0800, val.ttf);
}

void test_max17205_read_time_pwrup(void) {
    max17205_t dev = make_ready_dev();
    max17205_readings_t val;
    memset(&val, 0, sizeof(val));

    TEST_ASSERT_EQUAL_INT(max17205_arg_err, max17205_read_time_pwrup(NULL, &val));
    TEST_ASSERT_EQUAL_INT(max17205_arg_err, max17205_read_time_pwrup(&dev, NULL));

    i2c_fake_set_reg(GAUGE_MAIN, MAX17205_TIMERH_ADDR, 0x0123);
    TEST_ASSERT_EQUAL_INT(max17205_ok, max17205_read_time_pwrup(&dev, &val));
    TEST_ASSERT_EQUAL_UINT16(0x0123, val.time_pwrup);
}

void test_max17205_read_temperature(void) {
    max17205_t dev = make_ready_dev();
    max17205_readings_t val;
    memset(&val, 0, sizeof(val));

    TEST_ASSERT_EQUAL_INT(max17205_arg_err, max17205_read_temperature(NULL, &val));
    TEST_ASSERT_EQUAL_INT(max17205_arg_err, max17205_read_temperature(&dev, NULL));

    /* Temp: 1/256 degC per LSB, signed. 0x1900 = 6400 = 25.0 degC. */
    i2c_fake_set_reg(GAUGE_MAIN, MAX17205_TEMP_ADDR, 0x1900);
    TEST_ASSERT_EQUAL_INT(max17205_ok, max17205_read_temperature(&dev, &val));
    TEST_ASSERT_EQUAL_INT16(6400, val.temperature);
}

void test_max17205_read_temperature_ain1(void) {
    max17205_t dev = make_ready_dev();
    max17205_readings_t val;
    memset(&val, 0, sizeof(val));

    TEST_ASSERT_EQUAL_INT(max17205_arg_err, max17205_read_temperature_ain1(NULL, &val));
    TEST_ASSERT_EQUAL_INT(max17205_arg_err, max17205_read_temperature_ain1(&dev, NULL));

    /* Shadow RAM, so this also catches a read off the main I2C address. */
    i2c_fake_set_reg(GAUGE_SHADOW, MAX17205_TEMP1_ADDR, 0x1400); /* 20.0 degC */
    TEST_ASSERT_EQUAL_INT(max17205_ok, max17205_read_temperature_ain1(&dev, &val));
    TEST_ASSERT_EQUAL_INT16(5120, val.temperature_ain1);
}

void test_max17205_read_temperature_ain2(void) {
    max17205_t dev = make_ready_dev();
    max17205_readings_t val;
    memset(&val, 0, sizeof(val));

    TEST_ASSERT_EQUAL_INT(max17205_arg_err, max17205_read_temperature_ain2(NULL, &val));
    TEST_ASSERT_EQUAL_INT(max17205_arg_err, max17205_read_temperature_ain2(&dev, NULL));

    i2c_fake_set_reg(GAUGE_SHADOW, MAX17205_TEMP2_ADDR, 0x1E00); /* 30.0 degC */
    TEST_ASSERT_EQUAL_INT(max17205_ok, max17205_read_temperature_ain2(&dev, &val));
    TEST_ASSERT_EQUAL_INT16(7680, val.temperature_ain2);
}

void test_max17205_read_temperature_die(void) {
    max17205_t dev = make_ready_dev();
    max17205_readings_t val;
    memset(&val, 0, sizeof(val));

    TEST_ASSERT_EQUAL_INT(max17205_arg_err, max17205_read_temperature_die(NULL, &val));
    TEST_ASSERT_EQUAL_INT(max17205_arg_err, max17205_read_temperature_die(&dev, NULL));

    i2c_fake_set_reg(GAUGE_SHADOW, MAX17205_INTTEMP_ADDR, 0x2300); /* 35.0 degC */
    TEST_ASSERT_EQUAL_INT(max17205_ok, max17205_read_temperature_die(&dev, &val));
    TEST_ASSERT_EQUAL_INT16(8960, val.temperature_die);
}

void test_max17205_read_all(void) {
    max17205_t dev = make_ready_dev();
    max17205_readings_t val;
    memset(&val, 0, sizeof(val));

    TEST_ASSERT_EQUAL_INT(max17205_arg_err, max17205_read_all(NULL, &val));
    TEST_ASSERT_EQUAL_INT(max17205_arg_err, max17205_read_all(&dev, NULL));

    i2c_fake_set_reg(GAUGE_MAIN, MAX17205_REPSOC_ADDR, 0x3200);
    i2c_fake_set_reg(GAUGE_MAIN, MAX17205_CAPACITY_ADDR, 3000);
    i2c_fake_set_reg(GAUGE_MAIN, MAX17205_CURRENT_ADDR, 256);
    i2c_fake_set_reg(GAUGE_MAIN, MAX17205_VBAT_ADDR, 6400);
    i2c_fake_set_reg(GAUGE_MAIN, MAX17205_VCELL_ADDR, 40960);
    i2c_fake_set_reg(GAUGE_MAIN, MAX17205_AVCELL_ADDR, 100);
    i2c_fake_set_reg(GAUGE_MAIN, MAX17205_TTE_ADDR, 0x0400);
    i2c_fake_set_reg(GAUGE_MAIN, MAX17205_TTF_ADDR, 0x0800);
    i2c_fake_set_reg(GAUGE_MAIN, MAX17205_TIMERH_ADDR, 0x0123);
    i2c_fake_set_reg(GAUGE_MAIN, MAX17205_TEMP_ADDR, 0x1900);
    i2c_fake_set_reg(GAUGE_SHADOW, MAX17205_TEMP1_ADDR, 0x1400);
    i2c_fake_set_reg(GAUGE_SHADOW, MAX17205_TEMP2_ADDR, 0x1E00);
    i2c_fake_set_reg(GAUGE_SHADOW, MAX17205_INTTEMP_ADDR, 0x2300);

    TEST_ASSERT_EQUAL_INT(max17205_ok, max17205_read_all(&dev, &val));

    /* Every field distinct, so a field left at 0 or filled from the wrong
     * register shows up here. */
    TEST_ASSERT_EQUAL_UINT32(expect_soc(0x3200), val.soc);
    TEST_ASSERT_EQUAL_UINT32(expect_capacity(3000), val.capacity);
    TEST_ASSERT_EQUAL_INT32(expect_current(256), val.current);
    TEST_ASSERT_EQUAL_UINT32(expect_voltage(6400), val.voltage);
    TEST_ASSERT_EQUAL_UINT32(expect_midvoltage(40960), val.midvoltage);
    TEST_ASSERT_EQUAL_UINT16(100, val.cycles);
    TEST_ASSERT_EQUAL_UINT16(0x0400, val.tte);
    TEST_ASSERT_EQUAL_UINT16(0x0800, val.ttf);
    TEST_ASSERT_EQUAL_UINT16(0x0123, val.time_pwrup);
    TEST_ASSERT_EQUAL_INT16(6400, val.temperature);
    TEST_ASSERT_EQUAL_INT16(5120, val.temperature_ain1);
    TEST_ASSERT_EQUAL_INT16(7680, val.temperature_ain2);
    TEST_ASSERT_EQUAL_INT16(8960, val.temperature_die);
}

/* Reset test */
void test_max17205_reset(void) {
    max17205_t dev = make_ready_dev();

    TEST_ASSERT_EQUAL_INT(max17205_arg_err, max17205_reset(NULL));

    TEST_ASSERT_EQUAL_INT(max17205_ok, max17205_reset(&dev));
    TEST_ASSERT_EQUAL_HEX16_MESSAGE(MAX17205_CMD_FULL_RESET,
                                    i2c_fake_get_reg(GAUGE_MAIN, MAX17205_COMMAND_ADDR),
                                    "reset must write 0x000F to the Command register (0x60)");
}
