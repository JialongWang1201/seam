/* seam COBS encoder round-trip tests. SPDX-License-Identifier: MIT */
#include "seam_cobs.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define MAX_INPUT 1024u
#define MAX_ENCODED (MAX_INPUT + MAX_INPUT / 254u + 2u)

static int check_round_trip(const uint8_t *input, size_t length)
{
    uint8_t encoded[MAX_ENCODED];
    uint8_t decoded[MAX_INPUT];
    size_t encoded_length = seam_cobs_encode(input, length, encoded);

    if (encoded_length == 0 || encoded_length > sizeof(encoded))
        return -1;
    for (size_t i = 0; i < encoded_length; i++) {
        if (encoded[i] == 0)
            return -1;
    }

    size_t decoded_length = seam_cobs_decode(encoded, encoded_length,
                                             decoded, sizeof(decoded));
    if (decoded_length != length || memcmp(input, decoded, length) != 0)
        return -1;
    return 0;
}

static uint32_t next_random(uint32_t *state)
{
    *state ^= *state << 13;
    *state ^= *state >> 17;
    *state ^= *state << 5;
    return *state;
}

int main(void)
{
    uint8_t input[MAX_INPUT];
    uint8_t encoded[MAX_ENCODED];
    uint32_t random_state = 0x5eac0b5u;

    {
        uint8_t guard[2] = { 0xa5, 0x5a };
        const uint8_t one_byte[] = { 2, 0x42 };
        const uint8_t zero_byte[] = { 1, 1 };
        if (seam_cobs_decode(one_byte, sizeof(one_byte), guard, 0) !=
                SEAM_COBS_INVALID || guard[0] != 0xa5 ||
            seam_cobs_decode(one_byte, sizeof(one_byte), guard, 1) != 1 ||
            guard[0] != 0x42 || guard[1] != 0x5a ||
            seam_cobs_decode(zero_byte, sizeof(zero_byte), guard, 0) !=
                SEAM_COBS_INVALID)
            goto fail;
    }

    {
        static uint8_t oversized_input[9180];
        static uint8_t oversized_frame[9180 + 9180 / 254 + 2];
        static uint8_t bounded_output[8192 + 1];
        memset(oversized_input, 0x42, sizeof(oversized_input));
        size_t frame_length = seam_cobs_encode(oversized_input,
                                               sizeof(oversized_input),
                                               oversized_frame);
        bounded_output[8192] = 0xa5;
        if (seam_cobs_decode(oversized_frame, frame_length, bounded_output,
                             8192) != SEAM_COBS_INVALID ||
            bounded_output[8192] != 0xa5)
            goto fail;
    }

    if (seam_cobs_encode(input, 0, encoded) != 1 || encoded[0] != 1 ||
        check_round_trip(input, 0) != 0)
        goto fail;

    input[0] = 0;
    if (check_round_trip(input, 1) != 0)
        goto fail;
    input[0] = 0x42;
    if (check_round_trip(input, 1) != 0)
        goto fail;

    memset(input, 0x42, 254);
    if (seam_cobs_encode(input, 254, encoded) != 256 ||
        encoded[0] != 0xff || encoded[255] != 1 ||
        check_round_trip(input, 254) != 0)
        goto fail;
    input[254] = 0x43;
    if (check_round_trip(input, 255) != 0)
        goto fail;

    for (size_t trial = 0; trial < 1000; trial++) {
        size_t length = next_random(&random_state) % (MAX_INPUT + 1u);
        for (size_t i = 0; i < length; i++)
            input[i] = (uint8_t)next_random(&random_state);
        if (check_round_trip(input, length) != 0) {
            fprintf(stderr, "COBS round-trip failed on trial %zu\n", trial);
            return 1;
        }
    }
    return 0;

fail:
    fputs("COBS boundary round-trip failed\n", stderr);
    return 1;
}
