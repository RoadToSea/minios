/*
 * ************************************************
 *
 *        Debug probe implementation
 *
 *  debug_probe.c - see debug_probe.h for the exposed
 *  interfaces and watch-view symbols.
 *
 *  The two "call styles" this module demonstrates:
 *
 *    Normal call (thread/main mode)
 *      main -> DebugProbe_RunNormalCall -> (IRQ preempts here)
 *      TIM2_IRQHandler -> ProbeIrqBody -> return to the
 *      interrupted thread/main code.
 *
 *    IRQ-triggered call (handler mode)
 *      any code -> DebugProbe_TriggerIrq -> NVIC pends TIM2
 *      -> TIM2_IRQHandler -> ProbeIrqBody -> return.
 *
 *  Both paths share TIM2_IRQHandler + ProbeIrqBody, so the
 *  watch-view comparison between probeNormalRegs (thread
 *  mode) and probeIrqRegs (handler mode) shows exactly what
 *  the exception entry/exit changes: stack switch, IPSR,
 *  stacked registers in probeIrqFrame.
 *
 * ************************************************
 */

#include "debug_probe.h"

volatile DebugProbe_RegSnapshot probeNormalRegs;
volatile DebugProbe_RegSnapshot probeIrqRegs;
volatile DebugProbe_RegSnapshot probeAfterIrqRegs;
volatile DebugProbe_ExcFrame    probeIrqFrame;

volatile uint32_t probeRunRequest      = 1;
volatile uint32_t probeNormalCallCount;
volatile uint32_t probeIrqCallCount;
volatile uint32_t probeNormalResult;
volatile uint32_t probeIrqResult;

volatile uint32_t probeCallDepth;
volatile uint32_t probeCallChain[DEBUG_PROBE_CALL_CHAIN_MAX];

/* Sample SP/LR of the current context into a snapshot.
 * Separate noinline function so each capture is one clean
 * call the debugger can step into and show on the stack. */
__attribute__((noinline, used)) static void ProbeCaptureContext(volatile DebugProbe_RegSnapshot *snap)
{
    uint32_t stackPointer;
    uint32_t returnAddress;

    __asm volatile("mov %0, sp\nmov %1, lr"
                   : "=r"(stackPointer), "=r"(returnAddress));

    snap->sp      = stackPointer;
    snap->lr      = returnAddress;
    snap->ipsr    = __get_IPSR();
    snap->control = __get_CONTROL();
}

/* Record one return address in the IRQ-side call chain. */
__attribute__((noinline, used)) static void ProbeCallChainPush(uint32_t returnAddress)
{
    if (probeCallDepth < DEBUG_PROBE_CALL_CHAIN_MAX) {
        probeCallChain[probeCallDepth] = returnAddress;
    }
    probeCallDepth++;
}

/* Leaf function the IRQ body calls last: its 'step into' shows
 * handler mode executing ordinary code, and LR here still
 * points back into ProbeIrqWork. */
__attribute__((noinline, used)) static uint32_t ProbeIrqLeaf(uint32_t value)
{
    return value ^ 0x13579BDFU;
}

/* Second-level IRQ-side function: adds one more stack frame
 * between TIM2_IRQHandler / ProbeIrqBody and the leaf. */
__attribute__((noinline, used)) static uint32_t ProbeIrqWork(uint32_t value)
{
    ProbeCallChainPush((uint32_t)__builtin_return_address(0));
    return ProbeIrqLeaf(value) + 1U;
}

__attribute__((noinline, used)) void ProbeIrqBody(const DebugProbe_ExcFrame *frame, uint32_t exceptionReturn)
{
    /* Handler context: MSP active, IPSR = 16 + TIM2_IRQn */
    probeIrqRegs.sp      = (uint32_t)frame;
    probeIrqRegs.lr      = exceptionReturn;
    probeIrqRegs.ipsr    = __get_IPSR();
    probeIrqRegs.control = __get_CONTROL();

    probeIrqFrame.r0   = frame->r0;
    probeIrqFrame.r1   = frame->r1;
    probeIrqFrame.r2   = frame->r2;
    probeIrqFrame.r3   = frame->r3;
    probeIrqFrame.r12  = frame->r12;
    probeIrqFrame.lr   = frame->lr;
    probeIrqFrame.pc   = frame->pc;
    probeIrqFrame.xpsr = frame->xpsr;

    probeCallDepth = 0;
    probeIrqResult = ProbeIrqWork(0xAABBCCDDU);

    GPIOC->ODR ^= GPIO_Pin_13;
    probeIrqCallCount++;
}

__attribute__((naked)) void TIM2_IRQHandler(void)
{
    __asm volatile(
        "tst lr, #4\n"
        "ite eq\n"
        "mrseq r0, msp\n"
        "mrsne r0, psp\n"
        "mov r1, lr\n"
        "b ProbeIrqBody\n");
}

__attribute__((noinline)) uint32_t DebugProbe_RunNormalCall(uint32_t first, uint32_t second)
{
    uint32_t previousInterruptCount;

    /* Context before the IRQ fires */
    ProbeCaptureContext(&probeNormalRegs);
    probeNormalCallCount++;

    previousInterruptCount = probeIrqCallCount;
    NVIC_SetPendingIRQ(TIM2_IRQn);
    __DSB();
    __ISB();

    while (probeIrqCallCount == previousInterruptCount) {
        __NOP();
    }

    /* Context after the IRQ returned */
    ProbeCaptureContext(&probeAfterIrqRegs);

    return first + second;
}

void DebugProbe_TriggerIrq(void)
{
    NVIC_SetPendingIRQ(TIM2_IRQn);
    __DSB();
    __ISB();
}

void DebugProbe_RequestRun(void)
{
    probeRunRequest = 1;
}
