#include <assert.h>
#include <stdint.h>
#include <string.h>

#define SEAM_IMPLEMENT
#include "seam_agent.h"

static uint8_t output[sizeof(cfl_bundle_t) +
                      SEAM_RING_SIZE * sizeof(cfl_record_t) + 16u];
static size_t output_len;
static uint32_t critical_depth;

uint32_t seam_port_enter_critical(void)
{
    uint32_t previous = critical_depth;
    critical_depth++;
    return previous;
}

void seam_port_exit_critical(uint32_t previous)
{
    assert(critical_depth == previous + 1u);
    critical_depth = previous;
}

uint32_t seam_port_tick(void)
{
    assert(critical_depth == 1u);
    return _seam_seq;
}

void seam_port_write_block(const uint8_t *buf, size_t len)
{
    assert(critical_depth == 0u);
    assert(len <= sizeof(output));
    memcpy(output, buf, len);
    output_len = len;
}

static void check_bundle(uint32_t oldest, uint16_t last_seq)
{
    uint8_t decoded[sizeof(cfl_bundle_t) +
                    SEAM_RING_SIZE * sizeof(cfl_record_t)];
    seam_dump_bundle(last_seq);
    assert(output_len > 1u && output[output_len - 1u] == 0u);
    size_t length = seam_cobs_decode(output, output_len - 1u,
                                     decoded, sizeof(decoded));
    assert(length == sizeof(decoded));
    const cfl_bundle_t *bundle = (const cfl_bundle_t *)decoded;
    assert(bundle->n_records == SEAM_RING_SIZE);
    assert(bundle->fault_seq == last_seq);
    for (uint32_t i = 0; i < SEAM_RING_SIZE; i++) {
        assert(bundle->records[i].a == oldest + i);
        assert(bundle->records[i].seq == (uint16_t)(oldest + i));
    }
}

int main(void)
{
    for (uint32_t i = 0; i < 65536u; i++)
        seam_emit(CFL_LAYER_HW, CFL_EV_IRQ_FIRE, i, 0, 0, 0);
    assert(_seam_count == SEAM_RING_SIZE);
    check_bundle(65536u - SEAM_RING_SIZE, 65535u);

    seam_emit(CFL_LAYER_HW, CFL_EV_IRQ_FIRE, 65536u, 0, 0, 0);
    assert(_seam_seq == 1u);
    assert(_seam_count == SEAM_RING_SIZE);
    check_bundle(65537u - SEAM_RING_SIZE, 0u);
    assert(critical_depth == 0u);
    return 0;
}
