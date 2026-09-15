# STM32F103 learning project

This repository contains the Cortex-M3 bare-metal learning project for the STM32F103C8.
The `base` directory next to this repository is intentionally not managed by Git.

## Learning stages

The stages are represented by Git branches. Each later stage is based on the previous one,
so you can switch branches to compare the implementation and the generated execution model.

| Stage | Branch | Focus |
| --- | --- | --- |
| 1 | `stage-1-functions` | Ordinary C function calls, parameters, return values, and stack frames |
| 2 | `stage-2-interrupts` | TIM2 interrupt entry/return, exception stack frame, MSP, IPSR, and EXC_RETURN |
| 3 | `stage-3-task-switching` | PSP, task stack frames, PendSV, and first task activation |
| 4 | `stage-4-time-slicing` | SysTick-driven PendSV and round-robin time-slice scheduling |

`main` points to the complete fourth-stage implementation. `snapshot/final` preserves the
state that existed before the staged cleanup.

## Branch workflow

```powershell
git switch stage-1-functions
git switch stage-2-interrupts
git switch stage-3-task-switching
git switch stage-4-time-slicing
```

To inspect the progression:

```powershell
git log --oneline --graph --decorate --all
git diff stage-1-functions..stage-2-interrupts
git diff stage-2-interrupts..stage-3-task-switching
git diff stage-3-task-switching..stage-4-time-slicing
```

## External notes

The related Obsidian notes are kept outside this repository:

- `D:\obsidian\note\单片机\底层原理\函数调用\函数调用机制.md`
- `D:\obsidian\note\单片机\底层原理\中断\中断.md`
- `D:\obsidian\note\单片机\底层原理\启动流程\流程梳理.md`

## Build artifacts

`build/` and other generated output directories are ignored. The project is configured for
STM32-EIDE and uses the existing CMSIS, RTX, and STM32F10x peripheral sources under `lib/`.
