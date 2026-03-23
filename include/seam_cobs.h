/* seam_cobs.h — Consistent Overhead Byte Stuffing (COBS)
 *
 * Framing for .cfl bundles over UART/SWO. Frame delimiter: 0x00.
 * Worst-case expansion: 1 byte per 254 bytes of input + 1 overhead byte.
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef SEAM_COBS_H
#define SEAM_COBS_H

#include <stddef.h>
#include <stdint.h>

/* Portable sentinel for "not a valid length" — avoids SIZE_MAX which is
 * not reliably defined by <stdint.h> on all bare-metal Newlib toolchains. */
#ifndef SEAM_COBS_INVALID
#  define SEAM_COBS_INVALID ((size_t)-1)
#endif

/* Encode src[0..src_len) into dst. Returns encoded length (not including
 * the trailing 0x00 delimiter — caller appends it).
 * dst must be at least src_len + src_len/254 + 2 bytes. */
static inline size_t seam_cobs_encode(const uint8_t *src, size_t src_len,
                                      uint8_t *dst)
{
    size_t  di = 0;
    size_t  code_idx = 0;
    uint8_t code = 1;

    for (size_t si = 0; si < src_len; si++) {
        if (src[si] != 0x00) {
            dst[di++] = src[si];
            code++;
            if (code == 0xFF) {
                dst[code_idx] = code;
                code_idx = di;
                dst[di++] = 0x00; /* placeholder */
                code = 1;
            }
        } else {
            dst[code_idx] = code;
            code_idx = di;
            dst[di++] = 0x00; /* placeholder */
            code = 1;
        }
    }
    dst[code_idx] = code;
    return di;
}

/* Decode src[0..src_len) (no trailing 0x00) into dst.
 * Returns decoded length, or SEAM_COBS_INVALID on framing error. */
static inline size_t seam_cobs_decode(const uint8_t *src, size_t src_len,
                                      uint8_t *dst)
{
    size_t di = 0;
    size_t si = 0;

    while (si < src_len) {
        uint8_t code = src[si++];
        if (code == 0x00) return SEAM_COBS_INVALID; /* unexpected delimiter */
        for (uint8_t i = 1; i < code; i++) {
            if (si >= src_len) return SEAM_COBS_INVALID;
            dst[di++] = src[si++];
        }
        if (code < 0xFF && si < src_len)
            dst[di++] = 0x00;
    }
    return di;
}

#endif /* SEAM_COBS_H */
