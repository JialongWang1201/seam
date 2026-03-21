# seam — STM32F4 Integration Example

Minimal integration of `seam_agent.h` into an STM32F4 project (Cortex-M4,
FreeRTOS, HAL or bare-metal).  The same pattern works on any Cortex-M device
that has a UART and a DWT cycle counter.

---

## Step 1 — Add files to your project

Copy `seam_port_stm32f4.c` into your BSP directory.  Add these paths to your
build system:

```
include/              ← seam_agent.h, seam_fault_log.h, seam_cobs.h
examples/stm32f4/     ← seam_port_stm32f4.c
```

CMake example:

```cmake
target_sources(your_firmware PRIVATE
    path/to/seam/examples/stm32f4/seam_port_stm32f4.c
)
target_include_directories(your_firmware PRIVATE
    path/to/seam/include
)
```

## Step 2 — Call seam_port_init() at startup

In `main.c`, after UART init and before the RTOS scheduler starts:

```c
extern void seam_port_init(void);   /* defined in seam_port_stm32f4.c */

int main(void) {
    board_uart_init();
    seam_port_init();   /* enables DWT->CYCCNT */
    // ...
    vTaskStartScheduler();
}
```

## Step 3 — Emit events at interesting sites

In any source file, include `seam_agent.h` (without `SEAM_IMPLEMENT`):

```c
#include "seam_agent.h"

/* KDI throttle path */
void kdi_activate_throttle(uint32_t driver_id, uint32_t budget) {
    seam_emit(CFL_LAYER_KDI, CFL_EV_KDI_THROTTLE, driver_id, budget, 0, 0);
    // ...
}

/* VM policy violation path */
void vm_policy_fail(uint32_t addr) {
    seam_emit(CFL_LAYER_VM, CFL_EV_VM_POLICY_FAIL, addr, 0, 0, 0);
    // ...
}
```

## Step 4 — Dump from fault handler

```c
#include "seam_agent.h"

void MemManage_Handler(void) {
    uint32_t cfsr  = SCB->CFSR;
    uint32_t mmfar = SCB->MMFAR;

    /* Emit the fault anchor event */
    seam_emit(CFL_LAYER_HW, CFL_EV_FAULT_ENTRY, cfsr, mmfar, 0, 0);

    /* Serialize ring → COBS bundle → UART */
    seam_dump_bundle(_seam_seq - 1U);

    for (;;) {}   /* spin or reset */
}
```

## Step 5 — Analyze on the host

Capture the raw UART bytes to a file (e.g. with `mkdbg` or any serial logger),
then run:

```
seam-analyze capture.bin
```

Output:

```
CAUSAL CHAIN (confidence: 87%)
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
[T-3] KDI    ev=KDI_THROTTLE       seq=12   KDI throttle cascaded into VM MMIO policy window expiry
[T-2] VM     ev=VM_POLICY_FAIL     seq=13   KDI throttle cascaded into VM MMIO policy window expiry
[T-1] HW     ev=MPU_VIOLATION      seq=14   MemManage fault caused by MPU region violation
[T+0] HW     ev=FAULT_ENTRY        seq=15    ──────────
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
VERDICT: KDI throttle cascaded into VM MMIO policy window expiry
RECORDS: 16   BOARD_ID: 0x0000
```

---

## RAM budget

| `SEAM_RING_SIZE` | Ring buffer | Static buffers | Total |
|---|---|---|---|
| 16 (min) | 384 B | ~440 B | ~824 B |
| 32 (default) | 768 B | ~840 B | ~1608 B |
| 64 (max useful) | 1536 B | ~1640 B | ~3176 B |

Override the default: `-DSEAM_RING_SIZE=16` in your compile flags.

## FreeRTOS events (optional)

Include `seam_freertos.h` after your FreeRTOS headers to automatically
capture task-switch, queue-full, and task-block events:

```c
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "seam_freertos.h"  /* wires traceTASK_SWITCHED_IN etc. */
```
