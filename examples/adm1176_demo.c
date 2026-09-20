#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "pico/stdio_usb.h"
#include "pico/stdlib.h"

#include "argus/drivers/adm1176.h"

#ifndef ARGUS_ADM1176_I2C_ADDRESS
#define ARGUS_ADM1176_I2C_ADDRESS 0x40
#endif

#ifndef ARGUS_ADM1176_SENSE_RESISTOR_OHM
#define ARGUS_ADM1176_SENSE_RESISTOR_OHM ADM1176_DEFAULT_SENSE_RESISTOR_OHM
#endif

#ifndef ARGUS_ADM1176_I2C_SDA_PIN
#define ARGUS_ADM1176_I2C_SDA_PIN PICO_DEFAULT_I2C_SDA_PIN
#endif

#ifndef ARGUS_ADM1176_I2C_SCL_PIN
#define ARGUS_ADM1176_I2C_SCL_PIN PICO_DEFAULT_I2C_SCL_PIN
#endif

#ifndef ARGUS_ADM1176_I2C_INSTANCE
#define ARGUS_ADM1176_I2C_INSTANCE i2c1
#endif

#ifndef ARGUS_PERIPH_PWR_ENABLE_PIN
#define ARGUS_PERIPH_PWR_ENABLE_PIN 42
#endif

typedef struct {
    i2c_inst_t *instance;
    uint32_t timeout_us;
} pico_i2c_context_t;

static adm1176_result_t pico_i2c_write(
    void *context,
    uint8_t address,
    const uint8_t *data,
    size_t length
)
{
    pico_i2c_context_t *i2c = context;
    if (i2c == NULL || i2c->instance == NULL || data == NULL || length == 0) {
        return ADM1176_RESULT_INVALID_ARGUMENT;
    }

    const int count = i2c_write_timeout_us(
        i2c->instance,
        address,
        data,
        length,
        false,
        i2c->timeout_us
    );
    return count == (int)length
               ? ADM1176_RESULT_OK
               : ADM1176_RESULT_TRANSPORT_WRITE_FAILED;
}

static adm1176_result_t pico_i2c_read(
    void *context,
    uint8_t address,
    uint8_t *data,
    size_t length
)
{
    pico_i2c_context_t *i2c = context;
    if (i2c == NULL || i2c->instance == NULL || data == NULL || length == 0) {
        return ADM1176_RESULT_INVALID_ARGUMENT;
    }

    const int count = i2c_read_timeout_us(
        i2c->instance,
        address,
        data,
        length,
        false,
        i2c->timeout_us
    );
    return count == (int)length
               ? ADM1176_RESULT_OK
               : ADM1176_RESULT_TRANSPORT_READ_FAILED;
}

static const char *result_name(adm1176_result_t result)
{
    switch (result) {
        case ADM1176_RESULT_OK:
            return "OK";
        case ADM1176_RESULT_INVALID_ARGUMENT:
            return "invalid argument";
        case ADM1176_RESULT_NOT_INITIALIZED:
            return "not initialized";
        case ADM1176_RESULT_TRANSPORT_WRITE_FAILED:
            return "I2C write failed/no ACK";
        case ADM1176_RESULT_TRANSPORT_READ_FAILED:
            return "I2C read failed/no ACK";
        case ADM1176_RESULT_OUT_OF_RANGE:
            return "value out of range";
        default:
            return "unknown error";
    }
}

int main(void)
{
    stdio_init_all();

    while (!stdio_usb_connected()) {
        sleep_ms(100);
    }
    sleep_ms(250);

    printf("\n[boot] Argus ADM1176 continuous diagnostic\n");
    printf(
        "[config] address=0x%02x bus=I2C1 SDA=%u SCL=%u sample_period=1000 ms\n",
        ARGUS_ADM1176_I2C_ADDRESS,
        ARGUS_ADM1176_I2C_SDA_PIN,
        ARGUS_ADM1176_I2C_SCL_PIN
    );

    /* The peripheral rail must be enabled before accessing the ADM1176. */
    gpio_init(ARGUS_PERIPH_PWR_ENABLE_PIN);
    gpio_put(ARGUS_PERIPH_PWR_ENABLE_PIN, true);
    gpio_set_dir(ARGUS_PERIPH_PWR_ENABLE_PIN, GPIO_OUT);
    sleep_ms(100);
    printf(
        "[power] PERIPH_PWR_EN GPIO%u=HIGH; waited 100 ms\n",
        ARGUS_PERIPH_PWR_ENABLE_PIN
    );

    pico_i2c_context_t i2c_context = {
        .instance = ARGUS_ADM1176_I2C_INSTANCE,
        .timeout_us = 10000,
    };
    adm1176_transport_t transport = {
        .context = &i2c_context,
        .write = pico_i2c_write,
        .read = pico_i2c_read,
    };

    const uint32_t actual_baudrate = i2c_init(i2c_context.instance, 400000);
    gpio_set_function(ARGUS_ADM1176_I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(ARGUS_ADM1176_I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(ARGUS_ADM1176_I2C_SDA_PIN);
    gpio_pull_up(ARGUS_ADM1176_I2C_SCL_PIN);
    printf("[ok] I2C controller initialized at %lu Hz\n", (unsigned long)actual_baudrate);

    adm1176_t monitor;
    bool monitor_ready = false;
    unsigned long attempt = 0;

    while (true) {
        if (!monitor_ready) {
            ++attempt;
            const adm1176_result_t init_result = adm1176_init(
                &monitor,
                &transport,
                ARGUS_ADM1176_I2C_ADDRESS,
                ARGUS_ADM1176_SENSE_RESISTOR_OHM,
                ADM1176_VOLTAGE_RANGE_26V35
            );
            if (init_result != ADM1176_RESULT_OK) {
                printf(
                    "[waiting] attempt=%lu ADM1176 0x%02x: %s (%d); retrying\n",
                    attempt,
                    ARGUS_ADM1176_I2C_ADDRESS,
                    result_name(init_result),
                    init_result
                );
                sleep_ms(1000);
                continue;
            }

            sleep_us(ADM1176_TYPICAL_CONVERSION_TIME_US + 50u);
            monitor_ready = true;
            printf("[ready] ADM1176 acknowledged at 0x%02x\n", ARGUS_ADM1176_I2C_ADDRESS);
        }

        adm1176_sample_t sample;
        const adm1176_result_t read_result = adm1176_read(&monitor, &sample);
        if (read_result == ADM1176_RESULT_OK) {
            printf(
                "[sample] V=%.3f V I=%.3f A (raw V=%u I=%u)\n",
                sample.voltage_v,
                sample.current_a,
                sample.raw_voltage,
                sample.raw_current
            );
        } else {
            printf(
                "[error] ADM1176 read: %s (%d); reinitializing\n",
                result_name(read_result),
                read_result
            );
            monitor_ready = false;
        }
        sleep_ms(1000);
    }
}
