#define SEAM_IMPLEMENT
#include "seam_agent.h"

uint32_t seam_port_tick(void)
{
    return 0u;
}

void seam_port_write_block(const uint8_t *buf, size_t len)
{
    (void)buf;
    (void)len;
}

void seam_header_compile_probe(void)
{
    seam_emit(CFL_LAYER_HW, CFL_EV_FAULT_ENTRY, 0, 0, 0, 0);
    seam_dump_bundle((uint16_t)(_seam_seq - 1u));
}
