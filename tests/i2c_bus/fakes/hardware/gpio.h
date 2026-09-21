/**
 * @file hardware/gpio.h
 * @brief Host-side stand-in for the Pico SDK's hardware/gpio.h.
 *
 * Function values below are not the real RP2350 encodings; tests only compare
 * them against each other, never against hardware.
 */

#ifndef FAKE_HARDWARE_GPIO_H
#define FAKE_HARDWARE_GPIO_H

#include "pico/stdlib.h"

typedef enum gpio_function {
    GPIO_FUNC_XIP = 0,
    GPIO_FUNC_SPI = 1,
    GPIO_FUNC_UART = 2,
    GPIO_FUNC_I2C = 3,
    GPIO_FUNC_PWM = 4,
    GPIO_FUNC_SIO = 5,
    GPIO_FUNC_PIO0 = 6,
    GPIO_FUNC_PIO1 = 7,
    GPIO_FUNC_NULL = 0x1f,
} gpio_function_t;

#define GPIO_OUT 1
#define GPIO_IN 0

void gpio_set_function(uint gpio, gpio_function_t fn);
void gpio_pull_up(uint gpio);
void gpio_disable_pulls(uint gpio);
void gpio_init(uint gpio);
void gpio_set_dir(uint gpio, bool out);
void gpio_put(uint gpio, bool value);
bool gpio_get(uint gpio);

#endif /* FAKE_HARDWARE_GPIO_H */
