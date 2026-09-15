# Stage 3: first task switch

## Learning target

Understand how a task gets its own stack and how PendSV restores a prepared exception
frame to enter a task in Thread mode with `PSP`.

## Entry point

- `src/main.c`: builds one task control block and calls `time_slice_switch_first_task()`.
- `src/time_slice_core.c`: constructs the initial fake frame and implements PendSV.
- `src/tools/rlist.*`: task list support.

## Suggested observation

Break at `PendSV_Handler()` and inspect `PSP` before and after the restore. The first
switch restores `R4-R11` in software, sets `PSP` to the hardware frame, and returns with
`EXC_RETURN = 0xFFFFFFFD`, which resumes Thread mode using PSP at `TaskEntry()`.
