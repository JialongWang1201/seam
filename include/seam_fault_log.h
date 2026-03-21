/* seam_fault_log.h — Causal Fault Log (.cfl) format
 *
 * Shared between MCU (seam_agent.h) and host (libseam.h).
 * All fields little-endian. Do not modify without bumping CFL_VERSION.
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef SEAM_FAULT_LOG_H
#define SEAM_FAULT_LOG_H

#include <stdint.h>

#define CFL_MAGIC   0x4D414553u  /* "SEAM" */
#define CFL_VERSION 1u

/* ── Layer tags ─────────────────────────────────────────────────────────── */
typedef enum {
    CFL_LAYER_HW   = 0,   /* CPU fault, MPU, IRQ                */
    CFL_LAYER_RTOS = 1,   /* scheduler, queue, task state        */
    CFL_LAYER_VM   = 2,   /* VM opcode, stack, policy            */
    CFL_LAYER_KDI  = 3,   /* capability token, driver state      */
} cfl_layer_t;

/* ── Event types ────────────────────────────────────────────────────────── */
typedef enum {
    /* HW layer — field mapping: a=CFSR, b=MMFAR, c=PC, d=LR */
    CFL_EV_FAULT_ENTRY    = 0x01,
    /* HW layer — a=region_idx, b=access_type(0=r,1=w), c=fault_addr */
    CFL_EV_MPU_VIOLATION  = 0x02,
    /* HW layer — a=irq_num, b=nesting_depth */
    CFL_EV_IRQ_FIRE       = 0x03,

    /* RTOS layer — a=from_task_id, b=to_task_id */
    CFL_EV_TASK_SWITCH    = 0x10,
    /* RTOS layer — a=task_id, b=reason(0=queue,1=mutex,2=delay) */
    CFL_EV_TASK_BLOCK     = 0x11,
    /* RTOS layer — a=queue_handle, b=sender_task_id */
    CFL_EV_QUEUE_FULL     = 0x12,

    /* VM layer — a=opcode, b=vm_pc, c=stack_depth */
    CFL_EV_VM_OPCODE      = 0x20,
    /* VM layer — a=mmio_addr, b=policy_rule_id */
    CFL_EV_VM_POLICY_FAIL = 0x21,
    /* VM layer — a=direction(0=overflow,1=underflow), b=depth */
    CFL_EV_VM_STACK_FAULT = 0x22,

    /* KDI layer — a=driver_id, b=token */
    CFL_EV_KDI_TOKEN_EXP  = 0x30,
    /* KDI layer — a=driver_id, b=budget_remaining */
    CFL_EV_KDI_THROTTLE   = 0x31,
    /* KDI layer — a=driver_id, b=old_state, c=new_state */
    CFL_EV_KDI_STATE      = 0x32,
} cfl_event_type_t;

/* ── 20-byte event record ───────────────────────────────────────────────── */
typedef struct __attribute__((packed)) {
    uint8_t  layer;        /* cfl_layer_t                              */
    uint8_t  event;        /* cfl_event_type_t                         */
    uint16_t seq;          /* monotonic counter; wraps at 65535        */
    uint32_t timestamp;    /* SysTick ticks, relative to bundle start  */
    uint32_t a;
    uint32_t b;
    uint32_t c;
    uint32_t d;
} cfl_record_t;            /* sizeof == 20 */

/* ── Bundle header ──────────────────────────────────────────────────────── */
typedef struct __attribute__((packed)) {
    uint32_t     magic;       /* CFL_MAGIC                              */
    uint8_t      version;     /* CFL_VERSION                            */
    uint8_t      n_records;   /* number of records; max 255             */
    uint16_t     board_id;    /* user-defined; 0 = unknown              */
    uint16_t     fault_seq;   /* seq of the fault anchor record         */
    uint16_t     _pad;        /* reserved, write 0                      */
    cfl_record_t records[];   /* oldest first; fault record is last     */
} cfl_bundle_t;

#endif /* SEAM_FAULT_LOG_H */
