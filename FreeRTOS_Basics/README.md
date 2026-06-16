# FreeRTOS_Basics

First FreeRTOS project. Two tasks blinking different LEDs at different rates to prove
the scheduler works. No inter-task communication - just the foundation.

## What it does

- defaultTask toggles PD12 (green LED) every 500ms
- orangeTask toggles PD13 (orange LED) every 200ms
- Both run concurrently via FreeRTOS scheduler

## Key config decisions

- Interface: CMSIS_V2. ARM's wrapper over native FreeRTOS API. What CubeMX generates
  and what most STM32 codebases use.
- Timebase: TIM1 instead of SysTick. FreeRTOS owns SysTick for its scheduler heartbeat.
  HAL needs a separate timer or they conflict.
- USE_NEWLIB_REENTRANT enabled. Makes C standard library thread-safe by giving each task
  its own library context. Costs extra RAM per task.
- Stack size: 128 words (512 bytes) per task. Sufficient for simple GPIO toggle tasks.
- Both tasks osPriorityNormal. No priority competition needed at this stage.

## How FreeRTOS is structured in generated code

main() initializes HAL and GPIO, then:
1. osKernelInitialize() - sets up FreeRTOS internals
2. osThreadNew() - registers each task with the scheduler
3. osKernelStart() - hands control to scheduler permanently, never returns

Task functions each contain a for(;;) loop. osDelay() yields the CPU for the specified
duration - the task sleeps and the scheduler runs other tasks. This is the key difference
from HAL_Delay which spins and burns cycles.

## Gotchas

> [!WARNING]
> CubeMX places task functions in main.c when freertos.c is otherwise empty - check both
> files before concluding a function doesn't exist.

> [!WARNING]
> USE_NEWLIB_REENTRANT warning appears on first code generation - enable it before
> regenerating, do not ignore it.

> [!IMPORTANT]
> osDelay() != HAL_Delay(). HAL_Delay blocks the CPU spinning. osDelay yields the CPU
> to the scheduler and puts the task to sleep. Never use HAL_Delay inside a FreeRTOS task.

## J-Link Thread-Aware Debugging

This project was used to verify J-Link thread-aware debugging with SEGGER J-Link Pro.

**Wiring (J-Link 20-pin to Discovery CN2):**
- J-Link Pin 1 (VTref) → 3.3V pin on P1/P2 header (not CN2 Pin 1 - floating on this board)
- J-Link Pin 7 (SWDIO) → CN2 Pin 4 (SWDIO)
- J-Link Pin 9 (SWCLK) → CN2 Pin 2 (SWCLK)
- J-Link Pin 4 (GND) → CN2 Pin 3 (GND)

**Critical: CN3 jumpers must remain ON** when using J-Link on this board. The SWD
signals on CN2 are only properly driven when the onboard ST-LINK is active. VTref
must be supplied externally from the 3.3V header pin - CN2 Pin 1 is floating.

**CubeIDE configuration:**
- Debug Configurations → Debugger tab
- Debug probe: SEGGER J-Link
- Interface: SWD, 4000 kHz
- RTOS Kernel Awareness: Enable, RTOS Proxy, Driver: FreeRTOS

**FreeRTOS Task List view (Window → Show View → FreeRTOS Task List):**

| Task | Priority | State | Description |
|------|----------|-------|-------------|
| defaultTask | 24/24 | DELAYED | sleeping in osDelay(500) |
| orangeTask | 24/24 | DELAYED | sleeping in osDelay(200) |
| IDLE | 0/0 | RUNNING | FreeRTOS idle task |
| Tmr Svc | 2/2 | BLOCKED | FreeRTOS timer service |

Clicking any task in the list switches the call stack view to that task - you can
see exactly where in the code each task is paused simultaneously.

> [!TIP]
> The Min Free Stack column shows watermark stack usage per task - use this to
> right-size stack allocations instead of guessing.

## Lessons Learned

- RTOS does not give true parallelism. One core, one instruction at a time. It gives the
  illusion of parallelism through fast context switching between sleeping tasks.
- HAL_Delay is the enemy in an RTOS context. Any task calling it blocks the CPU for that
  duration. osDelay puts the task to sleep and lets the scheduler run something else.
- FreeRTOS owns SysTick. This is non-negotiable. Always move HAL timebase to a hardware
  timer when enabling FreeRTOS.
- Each task needs its own stack. 128 words is the CubeMX default. Stack overflow is a
  real failure mode to watch for as tasks get more complex.
- Thread-aware debugging shows all task states simultaneously - which tasks are running,
  blocked, delayed, and what each is waiting on. Invaluable for diagnosing deadlocks
  and starvation that are invisible with printf debugging alone.

## Logic analyzer

No external signals to probe on this project. Use the FreeRTOS Task List view in
CubeIDE with J-Link to observe task states - Running, Ready, Blocked, Delayed -
as the scheduler switches between them.