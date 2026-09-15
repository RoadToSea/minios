# Stage 4: time-slice round-robin scheduling

## Learning target

Observe periodic context switches driven by SysTick. Each tick requests PendSV, and the
scheduler saves the current task's software-saved registers before restoring the next
task in the circular list.

## Entry point

- `src/main.c`: creates `taskA` and `taskB`, then starts the scheduler.
- `src/time_slice_core.c`: configures SysTick and performs round-robin switching.
- `src/tools/rlist.*`: circular task list.

## Suggested observation

Watch `cntA` and `cntB` while stepping through `SysTick_Handler()` and `PendSV_Handler()`.
Inspect each task's `PSP` and `stack_ptr` to see the independent stacks and the repeating
A → B → A schedule.
