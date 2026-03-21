/* seam_freertos.h — Optional FreeRTOS trace hook shims
 *
 * Include AFTER FreeRTOS headers and AFTER seam_agent.h.
 * Wires scheduler events into the seam ring automatically.
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef SEAM_FREERTOS_H
#define SEAM_FREERTOS_H

#include "seam_agent.h"
#include "FreeRTOS.h"
#include "task.h"

/* Task switch: emit from_task / to_task IDs */
#define traceTASK_SWITCHED_IN() \
    seam_emit(CFL_LAYER_RTOS, CFL_EV_TASK_SWITCH, \
              (uint32_t)(uintptr_t)xTaskGetCurrentTaskHandle(), 0, 0, 0)

/* Task blocked: emit task ID + reason */
#define traceBLOCKED_ON_QUEUE_PEEK_FROM_ISR() \
    seam_emit(CFL_LAYER_RTOS, CFL_EV_TASK_BLOCK, \
              (uint32_t)(uintptr_t)xTaskGetCurrentTaskHandle(), 0, 0, 0)

/* Queue send failed (full): emit queue handle */
#define traceQUEUE_SEND_FAILED(pxQueue) \
    seam_emit(CFL_LAYER_RTOS, CFL_EV_QUEUE_FULL, \
              (uint32_t)(uintptr_t)(pxQueue), \
              (uint32_t)(uintptr_t)xTaskGetCurrentTaskHandle(), 0, 0)

#endif /* SEAM_FREERTOS_H */
