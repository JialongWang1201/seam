/* libseam.h — Host-side causal fault analysis API
 *
 * Pure C99. Zero external dependencies.
 *
 * Typical usage:
 *   uint8_t buf[4096];
 *   size_t  len = read_bundle_from_uart(buf, sizeof(buf));
 *   causal_chain_t chain;
 *   int rc = seam_analyze((cfl_bundle_t *)buf, len, &chain);
 *   if (rc == SEAM_OK) seam_print(&chain, stdout);
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef LIBSEAM_H
#define LIBSEAM_H

#include "seam_fault_log.h"
#include <stddef.h>
#include <stdio.h>

/* ── Return codes ───────────────────────────────────────────────────────── */
#define SEAM_OK                   0
#define SEAM_ERR_BAD_MAGIC       -1
#define SEAM_ERR_VERSION         -2
#define SEAM_ERR_TRUNCATED       -3
#define SEAM_ERR_NO_FAULT_ANCHOR -4

/* ── Causal chain ───────────────────────────────────────────────────────── */
#define SEAM_CHAIN_MAX 16

typedef struct {
    const cfl_record_t *record;      /* pointer into bundle (bundle must outlive chain) */
    const char         *cause;       /* static string literal — do NOT free             */
    uint8_t             confidence;  /* 0–100                                           */
} seam_node_t;

typedef struct {
    seam_node_t chain[SEAM_CHAIN_MAX];
    uint8_t     depth;               /* actual chain length (≤ SEAM_CHAIN_MAX)          */
    uint8_t     truncated;           /* 1 if events were dropped due to chain cap        */
    const char *verdict;             /* static string literal — do NOT free             */
    uint8_t     verdict_confidence;  /* 0–100; min(100, sum of node confidences)         */
} seam_chain_t;

/* ── API ────────────────────────────────────────────────────────────────── */

/* Analyze a .cfl bundle. bundle_len is the total byte length of the buffer.
 * On success, out->chain[0] is the oldest causal event,
 * out->chain[out->depth-1] is the fault anchor.
 * cause strings in out->chain[i].cause are static literals — no free needed.
 * Returns SEAM_OK or a negative error code. */
int seam_analyze(const cfl_bundle_t *bundle, size_t bundle_len,
                 seam_chain_t *out);

/* Print causal chain to `fp` in human-readable format. */
void seam_print(const seam_chain_t *chain, FILE *fp);

#endif /* LIBSEAM_H */
