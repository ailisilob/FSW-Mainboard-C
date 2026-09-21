/**
 * @file pico/binary_info.h
 * @brief Host-side stand-in. Binary info is a link-time/metadata feature with
 * no runtime behaviour to fake, so the macros collapse to nothing.
 */

#ifndef FAKE_PICO_BINARY_INFO_H
#define FAKE_PICO_BINARY_INFO_H

#define bi_decl(x)
#define bi_decl_if_func_used(x)
#define bi_2pins_with_func(p0, p1, func)

#endif /* FAKE_PICO_BINARY_INFO_H */
