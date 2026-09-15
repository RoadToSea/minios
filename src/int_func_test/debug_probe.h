/*
 * ************************************************
 *
 *        Debug probe: normal-call / IRQ-call
 *        register observation module
 *
 *  Extracted from main.c so both bare-metal code and
 *  RTOS threads can use the same interfaces.
 *
 *  Add the volatile symbols below to the debugger
 *  watch view to observe the two execution contexts:
 *
 *    probeNormalRegs    - thread/main context before the IRQ
 *                         (bare metal: IPSR = 0, CONTROL.SPSEL = 0 -> MSP
 *                          RTOS:      IPSR = 0, CONTROL.SPSEL = 1 -> PSP)
 *    probeIrqRegs       - handler context inside TIM2_IRQHandler
 *                         (IPSR = 16 + TIM2_IRQn = 44, CONTROL.SPSEL = 0 -> MSP)
 *    probeAfterIrqRegs  - thread/main context after the IRQ returned
 *    probeIrqFrame      - hardware-stacked frame of the interrupted code
 *                         (r0..r3 hold DebugProbe_RunNormalCall arguments)
 *    probeCallDepth     - current call-chain depth inside the probe
 *                         (walk probeCallChain in a memory view)
 *    probeCallChain     - captured return addresses of the nested calls
 *
 *  CPU: STM32F103C8
 *
 * ************************************************
 */

#ifndef DEBUG_PROBE_H
#define DEBUG_PROBE_H

#include <stdint.h>
#include "stm32f10x.h"

/* Snapshot of a code context (thread mode / main mode) */
typedef struct {
    uint32_t sp;
    uint32_t lr;
    uint32_t ipsr;
    uint32_t control;
} DebugProbe_RegSnapshot;

/* Exception stack frame pushed by the hardware when TIM2 IRQ fired */
typedef struct {
    uint32_t r0;
    uint32_t r1;
    uint32_t r2;
    uint32_t r3;
    uint32_t r12;
    uint32_t lr;
    uint32_t pc;
    uint32_t xpsr;
} DebugProbe_ExcFrame;

/* Deepest call chain the IRQ-side probe can record */
#define DEBUG_PROBE_CALL_CHAIN_MAX 8U

/* Snapshots - add these symbols to the debugger watch view */
extern volatile DebugProbe_RegSnapshot probeNormalRegs;
extern volatile DebugProbe_RegSnapshot probeIrqRegs;
extern volatile DebugProbe_RegSnapshot probeAfterIrqRegs;
extern volatile DebugProbe_ExcFrame    probeIrqFrame;

extern volatile uint32_t probeRunRequest;       /* set to 1 to request one probe run      */
extern volatile uint32_t probeNormalCallCount;  /* number of started normal calls         */
extern volatile uint32_t probeIrqCallCount;     /* number of executed IRQ bodies          */
extern volatile uint32_t probeNormalResult;     /* result of the last normal call         */
extern volatile uint32_t probeIrqResult;        /* marker written inside the IRQ body     */

/* Call-chain recording inside the IRQ body (watch in a memory view) */
extern volatile uint32_t probeCallDepth;                       /* entries used            */
extern volatile uint32_t probeCallChain[DEBUG_PROBE_CALL_CHAIN_MAX]; /* return addresses  */

/* Interfaces ------------------------------------------------------------ */

/* Normal-mode call: captures the current context, triggers the TIM2 IRQ,
 * waits for the IRQ body to finish, captures the context again and returns
 * first + second.
 * Note: concurrent callers may interleave the snapshot writes; serialize
 * with a mutex (see rtos_test.c) when called from several threads. */
uint32_t DebugProbe_RunNormalCall(uint32_t first, uint32_t second);

/* Interrupt-triggered call: pends the TIM2 IRQ once, without any
 * surrounding function-call observation. */
void DebugProbe_TriggerIrq(void);

/* Request one probe run (same as writing probeRunRequest = 1 in the
 * debugger watch view). */
void DebugProbe_RequestRun(void);

#endif /* DEBUG_PROBE_H */
