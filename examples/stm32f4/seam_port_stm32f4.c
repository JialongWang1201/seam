/* examples/stm32f4/seam_port_stm32f4.c
 *
 * Reference BSP port for STM32F4 (Cortex-M4 + USART2).
 * Copy this file into your project and adjust SEAM_UART to match your
 * UART instance.
 *
 * Tick source  — DWT cycle counter (CYCCNT).  Runs independently of the
 *                RTOS, SysTick, and fault state.  Always enabled after
 *                seam_port_init().
 *
 * Transport    — Busy-wait UART TX.  Intentionally does not use DMA or
 *                interrupts so it remains safe to call from HardFault and
 *                MemManage handlers when the RTOS is frozen.
 *
 * Storage      — Define SEAM_IMPLEMENT in this file (and ONLY this file)
 *                so the shared ring buffer lives here.  All other files
 *                that call seam_emit() include seam_agent.h without it.
 *
 * SPDX-License-Identifier: MIT
 */

/* Must come before the #include — defines ring buffer storage */
#define SEAM_IMPLEMENT
#include "seam_agent.h"

#include "stm32f4xx.h"   /* CoreDebug, DWT, USART_TypeDef, USART_SR_TXE … */

/* ── Choose your UART instance ───────────────────────────────────────────── */
#ifndef BOARD_UART_PORT
#define BOARD_UART_PORT 2
#endif

#if   BOARD_UART_PORT == 1
#  define SEAM_UART USART1
#elif BOARD_UART_PORT == 2
#  define SEAM_UART USART2
#else
#  define SEAM_UART USART3
#endif

/* ── seam_port_init: enable DWT cycle counter ────────────────────────────── */
void seam_port_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT       = 0U;
    DWT->CTRL        |= DWT_CTRL_CYCCNTENA_Msk;
}

/* ── seam_port_tick: DWT cycle counter ───────────────────────────────────── */
uint32_t seam_port_tick(void)
{
    return DWT->CYCCNT;
}

/* ── seam_port_write_block: busy-wait UART TX ────────────────────────────── */
void seam_port_write_block(const uint8_t *buf, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        while ((SEAM_UART->SR & USART_SR_TXE) == 0U) {}
        SEAM_UART->DR = (uint16_t)buf[i];
    }
    /* Wait for last byte to fully shift out */
    while ((SEAM_UART->SR & USART_SR_TC) == 0U) {}
}
