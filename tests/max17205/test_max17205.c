/**
 * @file test_max17205.c
 * @brief Unity unit tests for src/drivers/max17205.c.
 *
 * This is a test-first suite: it describes the interface as i2c_bus.h declares
 * it, so a number of these tests fail against the current implementation.
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

#include <stdio.h>   /* snprintf, for assertion messages */
#include <string.h>

/* Reserved I2C address ranges: 0x00-0x07 and 0x78-0x7F. */
#define FIRST_VALID_ADDR 0x08
#define LAST_VALID_ADDR  0x77
#define VALID_ADDR_COUNT 112

void setUp(void) {
}

void tearDown(void) {
}

/** 
 * 
 * max17205 initialization tests 
 * 
 * */
void test_max17205_init_rejects_null(void) {

}

void test_max17205_init_rejects_uninit_bus(void) {

}

void test_max17205_deinit_rejects_null(void) {

}

/* max17205 read value tests */
void test_max17205_read_soc(void) {
    
}

void test_max17205_read_capacity(void) {
    
}

void test_max17205_read_current(void) {
    
}

void test_max17205_read_voltage(void) {
    
}

void test_max17205_read_midvoltage(void) {
    
}

void test_max17205_read_cycles(void) {
    
}

void test_max17205_read_tte(void) {
    
}

void test_max17205_read_ttf(void) {
    
}

void test_max17205_read_time_pwrup(void) {
    
}

void test_max17205_read_temperature(void) {
    
}

void test_max17205_read_temperature_ain1(void) {
    
}

void test_max17205_read_temperature_ain2(void) {
    
}

void test_max17205_read_temperature_die(void) {
    
}

void test_max17205_read_all(void) {
    
}

/* Reset test */
void test_max17205_reset(void) {
    
}