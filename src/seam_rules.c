/* seam_rules.c — Built-in causal rule table
 *
 * Each rule matches a (trigger, precursor) event pair within a time window.
 * Rules are evaluated in order; first match wins per trigger event.
 *
 * SPDX-License-Identifier: MIT
 */
#include "seam_fault_log.h"
#include "seam_rules.h"

const seam_rule_t SEAM_RULES[] = {
    /* ── Hardware faults ──────────────────────────────────────────────── */

    /* MPU violation always immediately precedes a MemManage fault entry */
    {
        .trigger        = CFL_EV_FAULT_ENTRY,
        .precursor      = CFL_EV_MPU_VIOLATION,
        .tick_window    = 50,
        .same_actor     = 0,
        .cause          = "MemManage fault caused by MPU region violation",
        .confidence_add = 50,
    },

    /* ── KDI → VM cascades ────────────────────────────────────────────── */

    /* KDI throttle starved the driver budget, VM then hit policy window */
    {
        .trigger        = CFL_EV_VM_POLICY_FAIL,
        .precursor      = CFL_EV_KDI_THROTTLE,
        .tick_window    = 500,
        .same_actor     = 1,   /* same driver_id in field a */
        .cause          = "KDI throttle cascaded into VM MMIO policy window expiry",
        .confidence_add = 40,
    },

    /* KDI token TTL expiry caused VM to lose its MMIO access window */
    {
        .trigger        = CFL_EV_VM_POLICY_FAIL,
        .precursor      = CFL_EV_KDI_TOKEN_EXP,
        .tick_window    = 1000,
        .same_actor     = 1,
        .cause          = "VM MMIO policy window expired after KDI token TTL expiry",
        .confidence_add = 35,
    },

    /* ── RTOS cascades ────────────────────────────────────────────────── */

    /* Queue backed up, sender task eventually blocked */
    {
        .trigger        = CFL_EV_TASK_BLOCK,
        .precursor      = CFL_EV_QUEUE_FULL,
        .tick_window    = 200,
        .same_actor     = 0,   /* queue_handle != task_id, can't compare */
        .cause          = "Task blocked due to full queue",
        .confidence_add = 30,
    },

    /* KDI driver state went to ERROR right before task switch */
    {
        .trigger        = CFL_EV_TASK_SWITCH,
        .precursor      = CFL_EV_KDI_STATE,
        .tick_window    = 100,
        .same_actor     = 0,
        .cause          = "Task rescheduled following KDI driver state transition",
        .confidence_add = 20,
    },

    /* ── VM internal faults ───────────────────────────────────────────── */

    /* VM stack fault escalated into a hardware fault */
    {
        .trigger        = CFL_EV_FAULT_ENTRY,
        .precursor      = CFL_EV_VM_STACK_FAULT,
        .tick_window    = 100,
        .same_actor     = 0,
        .cause          = "Hardware fault triggered by VM stack overflow/underflow",
        .confidence_add = 45,
    },
};

const size_t SEAM_RULES_COUNT =
    sizeof(SEAM_RULES) / sizeof(SEAM_RULES[0]);
