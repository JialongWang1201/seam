/* seam_agent.h — MCU-side causal event collector
 *
 * Header-only. Zero RTOS dependency. Zero heap usage.
 * Static RAM: sizeof(cfl_record_t) * SEAM_RING_SIZE (default: 640 bytes).
 *
 * Quick start (3 steps):
 *   1. Implement seam_port_tick() and seam_port_write_block()
 *   2. Call seam_emit() at fault/RTOS/VM/KDI event sites
 *   3. Call seam_dump_bundle() from your fault handler
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef SEAM_AGENT_H
#define SEAM_AGENT_H

#include "seam_fault_log.h"
#include "seam_cobs.h"
#include <string.h>
#include <assert.h>

#ifndef SEAM_RING_SIZE
#define SEAM_RING_SIZE 32u
#endif

static_assert(SEAM_RING_SIZE <= 255u, "SEAM_RING_SIZE must fit in uint8_t");

/* ── Internal ring state ─────────────────────────────────────────────────
 *
 * Define SEAM_IMPLEMENT in exactly ONE translation unit before including
 * this header (typically your BSP port file, e.g. seam_port.c).
 * All other TUs that call seam_emit() just include this header and share
 * the same ring via external linkage.
 *
 *   seam_port.c:           #define SEAM_IMPLEMENT
 *                          #include "seam_agent.h"
 *
 *   fault.c / kdi.c / ...: #include "seam_agent.h"   // extern decls only
 */
#ifdef SEAM_IMPLEMENT
cfl_record_t _seam_ring[SEAM_RING_SIZE];
uint16_t     _seam_head = 0u; /* next write slot (mod SEAM_RING_SIZE) */
uint16_t     _seam_seq  = 0u; /* monotonic event counter              */
#else
extern cfl_record_t _seam_ring[];
extern uint16_t     _seam_head;
extern uint16_t     _seam_seq;
#endif

/* ── Port functions — implement these two in your BSP ──────────────────── */

/* Return current tick (e.g. SysTick->VAL, DWT->CYCCNT, or HAL_GetTick()). */
extern uint32_t seam_port_tick(void);

/* Write `len` bytes to transport (UART, SWO, etc.). Called once per bundle. */
extern void seam_port_write_block(const uint8_t *buf, size_t len);

/* ── Emit one event into the ring ───────────────────────────────────────── */
static inline void seam_emit(cfl_layer_t layer, cfl_event_type_t ev,
                              uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
    cfl_record_t *r = &_seam_ring[_seam_head % SEAM_RING_SIZE];
    r->layer     = (uint8_t)layer;
    r->event     = (uint8_t)ev;
    r->seq       = _seam_seq++;
    r->timestamp = seam_port_tick();
    r->a = a; r->b = b; r->c = c; r->d = d;
    _seam_head++;
}

/* ── Serialize ring → .cfl bundle → COBS-frame → write via port ─────────
 *
 * Call from your fault handler: seam_dump_bundle(_seam_seq - 1);
 * Uses two static buffers — not re-entrant, which is acceptable because
 * only one fault fires at a time on Cortex-M.
 */
static inline void seam_dump_bundle(uint16_t fault_seq)
{
    uint8_t n = (uint8_t)(_seam_head < SEAM_RING_SIZE
                          ? _seam_head : SEAM_RING_SIZE);

    /* Raw bundle buffer */
    static uint8_t _raw[sizeof(cfl_bundle_t) +
                         SEAM_RING_SIZE * sizeof(cfl_record_t)];
    cfl_bundle_t *b = (cfl_bundle_t *)_raw;

    b->magic     = CFL_MAGIC;
    b->version   = CFL_VERSION;
    b->n_records = n;
    b->board_id  = 0;
    b->fault_seq = fault_seq;
    b->_pad      = 0;

    /* Linearize ring oldest-first */
    uint16_t start = (_seam_head >= SEAM_RING_SIZE)
                     ? (_seam_head % SEAM_RING_SIZE) : 0u;
    for (uint8_t i = 0; i < n; i++)
        b->records[i] = _seam_ring[(start + i) % SEAM_RING_SIZE];

    size_t raw_len = sizeof(cfl_bundle_t) + n * sizeof(cfl_record_t);

    /* COBS-encode into second static buffer */
    static uint8_t _cobs[sizeof(_raw) + sizeof(_raw) / 254u + 4u];
    size_t cobs_len = seam_cobs_encode(_raw, raw_len, _cobs);
    _cobs[cobs_len++] = 0x00; /* COBS end-of-frame delimiter */

    seam_port_write_block(_cobs, cobs_len);
}

/* ── Optional: FreeRTOS trace hook shims ────────────────────────────────
 * Include seam_freertos.h after FreeRTOS headers to auto-wire RTOS events.
 */

#endif /* SEAM_AGENT_H */
