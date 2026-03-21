/* seam_rules.h — Internal rule table interface (not public API)
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef SEAM_RULES_H
#define SEAM_RULES_H

#include "seam_fault_log.h"
#include <stddef.h>

typedef struct {
    cfl_event_type_t trigger;        /* event that starts the match          */
    cfl_event_type_t precursor;      /* must appear before trigger in window */
    uint32_t         tick_window;    /* max timestamp delta; 0 = any         */
    uint8_t          same_actor;     /* 1 = field `a` must match in both     */
    const char      *cause;          /* static string literal                */
    uint8_t          confidence_add; /* base confidence contribution (0-100) */
} seam_rule_t;

extern const seam_rule_t SEAM_RULES[];
extern const size_t      SEAM_RULES_COUNT;

#endif /* SEAM_RULES_H */
