# Stage 1: ordinary function calls

## Learning target

Understand how a normal `BL` call passes arguments, creates a software stack frame,
and returns through `LR`. No interrupt or scheduler is enabled in this stage.

## Entry point

- `src/main.c`: `AddValues()` is called from `main()`.
- `function_result`: inspect the returned value in the debugger.

## Suggested observation

Set breakpoints at the call site and inside `AddValues()`. Compare `PC`, `LR`, `SP`,
`MSP`, and the values in `R0`/`R1`. A normal function call does not cause hardware
exception stacking or an MSP/PSP switch.
