/* seam_analyze.c — Causal fault analysis engine
 *
 * Algorithm (forward pass):
 *   chain[0] = oldest causal event
 *   chain[depth-1] = fault anchor (seq == bundle.fault_seq)
 *
 * For each record R (oldest → fault anchor):
 *   Scan SEAM_RULES for a matching trigger.
 *   If found, scan backwards for a matching precursor within tick_window.
 *   If both match (+ same_actor constraint), append a node to the chain.
 *
 * Confidence per node:
 *   base = rule.confidence_add
 *   +10 if same_actor matched
 *   +5  if a corroborating record exists in the window
 *   capped at 100
 *
 * SPDX-License-Identifier: MIT
 */
#include "libseam.h"
#include "seam_rules.h"
#include <string.h>
#include <stdint.h>

/* Timestamp delta (both are relative SysTick values in the bundle) */
static inline uint32_t tick_delta(uint32_t early, uint32_t late)
{
    return (late >= early) ? (late - early) : (UINT32_MAX - early + late + 1u);
}

int seam_analyze(const cfl_bundle_t *bundle, size_t bundle_len,
                 seam_chain_t *out)
{
    /* ── Validate header ────────────────────────────────────────────── */
    if (!bundle || bundle_len < sizeof(cfl_bundle_t))
        return SEAM_ERR_TRUNCATED;
    if (bundle->magic != CFL_MAGIC)
        return SEAM_ERR_BAD_MAGIC;
    if (bundle->version != CFL_VERSION)
        return SEAM_ERR_VERSION;

    size_t expected = sizeof(cfl_bundle_t) +
                      (size_t)bundle->n_records * sizeof(cfl_record_t);
    if (bundle_len < expected)
        return SEAM_ERR_TRUNCATED;

    uint8_t n = bundle->n_records;

    /* ── Locate fault anchor ────────────────────────────────────────── */
    int fault_idx = -1;
    for (int i = 0; i < (int)n; i++) {
        if (bundle->records[i].seq == bundle->fault_seq) {
            fault_idx = i;
            break;
        }
    }
    if (fault_idx < 0)
        return SEAM_ERR_NO_FAULT_ANCHOR;

    /* ── Initialize output ──────────────────────────────────────────── */
    memset(out, 0, sizeof(*out));

    /* ── Forward pass: oldest record → fault anchor ─────────────────── */
    for (int ri = 0; ri <= fault_idx; ri++) {
        const cfl_record_t *R = &bundle->records[ri];

        for (size_t rule_i = 0; rule_i < SEAM_RULES_COUNT; rule_i++) {
            const seam_rule_t *rule = &SEAM_RULES[rule_i];

            if (R->event != (uint8_t)rule->trigger)
                continue;

            /* Scan backwards for matching precursor */
            const cfl_record_t *P = NULL;
            for (int pi = ri - 1; pi >= 0; pi--) {
                const cfl_record_t *cand = &bundle->records[pi];
                if (cand->event != (uint8_t)rule->precursor)
                    continue;
                if (rule->tick_window != 0 &&
                    tick_delta(cand->timestamp, R->timestamp) > rule->tick_window)
                    break; /* records are oldest-first, delta only grows */
                if (rule->same_actor && cand->a != R->a)
                    continue;
                P = cand;
                break;
            }
            if (!P)
                continue;

            /* Compute confidence */
            uint8_t conf = rule->confidence_add;
            if (rule->same_actor)
                conf = (uint8_t)(conf + 10u > 100u ? 100u : conf + 10u);

            /* Corroboration: any additional record of precursor type in window */
            for (int pi2 = 0; pi2 < ri; pi2++) {
                if (pi2 == (int)(P - bundle->records)) continue;
                const cfl_record_t *c2 = &bundle->records[pi2];
                if (c2->event == (uint8_t)rule->precursor &&
                    (rule->tick_window == 0 ||
                     tick_delta(c2->timestamp, R->timestamp) <= rule->tick_window)) {
                    conf = (uint8_t)(conf + 5u > 100u ? 100u : conf + 5u);
                    break;
                }
            }

            /* Append node (cap at SEAM_CHAIN_MAX) */
            if (out->depth < SEAM_CHAIN_MAX) {
                out->chain[out->depth].record     = R;
                out->chain[out->depth].cause      = rule->cause;
                out->chain[out->depth].confidence = conf;
                out->depth++;
            } else {
                out->truncated = 1;
            }
            break; /* first matching rule wins */
        }
    }

    /* ── Ensure fault anchor is always the last node ────────────────── */
    if (out->depth == 0 ||
        out->chain[out->depth - 1].record != &bundle->records[fault_idx]) {
        if (out->depth < SEAM_CHAIN_MAX) {
            out->chain[out->depth].record     = &bundle->records[fault_idx];
            out->chain[out->depth].cause      = "fault anchor";
            out->chain[out->depth].confidence = 0;
            out->depth++;
        } else {
            out->truncated = 1;
        }
    }

    /* ── Verdict: highest-confidence node's cause ───────────────────── */
    uint8_t best_conf = 0;
    out->verdict = "unknown fault";
    for (uint8_t i = 0; i < out->depth; i++) {
        if (out->chain[i].confidence > best_conf) {
            best_conf    = out->chain[i].confidence;
            out->verdict = out->chain[i].cause;
        }
    }

    /* ── Verdict confidence: capped sum of all node confidences ─────── */
    uint32_t total = 0;
    for (uint8_t i = 0; i < out->depth; i++)
        total += out->chain[i].confidence;
    out->verdict_confidence = (uint8_t)(total > 100u ? 100u : total);

    return SEAM_OK;
}
