/**
 * @file pico/sync.h
 * @brief Host-side stand-in for the Pico SDK's pico/sync.h (mutex subset).
 *
 * The real mutex_t wraps a lock_core_t plus an owner id. The fake keeps the
 * same shape from the caller's point of view (opaque struct, same function
 * names) but stores plain bookkeeping the tests can read back.
 */

#ifndef FAKE_PICO_SYNC_H
#define FAKE_PICO_SYNC_H

#include "pico/stdlib.h"

typedef struct mutex {
    bool initialized;
    int  enter_count; /* incremented on enter, decremented on exit */
    int  owner;
} mutex_t;

void mutex_init(mutex_t *mtx);
bool mutex_is_initialized(mutex_t *mtx);
void mutex_enter_blocking(mutex_t *mtx);
bool mutex_try_enter(mutex_t *mtx, uint32_t *owner_out);
bool mutex_enter_timeout_us(mutex_t *mtx, uint32_t timeout_us);
void mutex_exit(mutex_t *mtx);

#endif /* FAKE_PICO_SYNC_H */
