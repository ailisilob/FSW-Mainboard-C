/**
 * @file pico/stdlib.h
 * @brief Host-side stand-in for the Pico SDK's pico/stdlib.h.
 *
 * Only the pieces i2c_bus.c actually touches are provided. Calls are recorded
 * in pico_fake (see fakes/pico_fake.h) so tests can assert on them.
 */

#ifndef FAKE_PICO_STDLIB_H
#define FAKE_PICO_STDLIB_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef unsigned int uint;

/* pico/error.h */
#define PICO_OK 0
#define PICO_ERROR_NONE 0
#define PICO_ERROR_GENERIC (-1)
#define PICO_ERROR_TIMEOUT (-2)

bool stdio_init_all(void);
void sleep_ms(uint32_t ms);
void sleep_us(uint64_t us);
uint32_t time_us_32(void);

#endif /* FAKE_PICO_STDLIB_H */
