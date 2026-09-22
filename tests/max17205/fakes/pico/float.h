/**
 * @file pico/float.h
 * @brief Host-side stand-in for the Pico SDK's pico/float.h.
 *
 * The real header swaps in the SDK's optimised single-precision math routines.
 * On the host the C library's own are already correct, so there is nothing to
 * fake -- this exists only so that max17205.h's `#include "pico/float.h"`
 * resolves off-target.
 */

#ifndef FAKE_PICO_FLOAT_H
#define FAKE_PICO_FLOAT_H

#include <math.h>

#endif /* FAKE_PICO_FLOAT_H */
