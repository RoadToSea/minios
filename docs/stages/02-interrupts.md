# Stage 2: interrupt entry and return

## Learning target

Observe how a TIM2 IRQ preempts thread mode and how Cortex-M hardware saves and restores
the interrupted register frame. Compare `MSP`, `IPSR`, `LR`/`EXC_RETURN`, and the
hardware-stacked frame in the debugger.

## Entry point

- `src/main.c`: configures TIM2 and calls `DebugProbe_RunNormalCall()`.
- `src/int_func_test/debug_probe.c`: captures thread and handler context.
- `src/stm32f10x_it.c`: exception handler templates.

## Suggested observation

Break in `ProbeIrqBody()`. Compare `probeNormalRegs`, `probeIrqRegs`, `probeIrqFrame`,
and `probeAfterIrqRegs`. The normal call changes `LR` with `BL`; exception entry adds a
hardware frame and uses an `EXC_RETURN` value to return to the interrupted context.
