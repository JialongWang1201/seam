# seam

A cross-layer causal fault analysis framework for embedded systems.

Because a register dump is not a root cause.

---

When an embedded system faults, you typically get this:

```
CFSR = 0x00000082
MMFAR = 0x20000400
PC = 0x08012344
```

**seam** gives you this:

```
CAUSAL CHAIN (confidence: 87%)
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
[T-5] KDI:  SENSOR token TTL expired      → budget 0/s
[T-4] RTOS: sensor_task blocked on queue  → queue full (16/16)
[T-3] VM:   STORE 0x20000400 attempted    → outside policy window
[T-2] HW:   MPU region 3 violation        → unprivileged write
[T-1] HW:   MemManage fault               → PC=0x08012344
[T-0] FAULT ANCHOR ──────────────────────────────────────
VERDICT: KDI throttle cascaded into VM MMIO policy window expiry.
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
```

---

## Architecture

```
┌─────────────────────────────────────┐
│              MCU                    │
│  fault.c  kdi.c  vm32.c  RTOS      │
│            seam_agent.h             │  ← one header, two port functions
│         UART / SWO (COBS)           │
└──────────────┬──────────────────────┘
               │  .cfl bundle
┌──────────────▼──────────────────────┐
│              HOST                   │
│            libseam                  │  ← pure C99, zero dependencies
│         causal_analyze()            │
│           mkdbg / CI                │
└─────────────────────────────────────┘
```

## Design

- **MCU side** — `include/seam_agent.h`, header-only, < 640 bytes RAM, zero RTOS dependency
- **Host side** — `libseam`, pure C99, zero external dependencies
- **Format** — `.cfl` (Causal Fault Log), 20 bytes/record, little-endian, COBS-framed over UART

## Status

Early development. See `include/seam_fault_log.h` for the format spec.

## License

MIT
