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

- CubeMX places task functions in main.c when freertos.c is otherwise empty - check both
- USE_NEWLIB_REENTRANT warning appears on first code generation - enable it before
  regenerating, do not ignore it
- osDelay() != HAL_Delay(). HAL_Delay blocks. osDelay yields.

## Lessons Learned

- RTOS does not give true parallelism. One core, one instruction at a time. It gives the
  illusion of parallelism through fast context switching between sleeping tasks.
- HAL_Delay is the enemy in an RTOS context. Any task calling it blocks the CPU for that
  duration. osDelay puts the task to sleep and lets the scheduler run something else.
- FreeRTOS owns SysTick. This is non-negotiable. Always move HAL timebase to a hardware
  timer when enabling FreeRTOS.
- Each task needs its own stack. 128 words is the CubeMX default. Stack overflow is a
  real failure mode to watch for as tasks get more complex.

## Logic analyzer

No external signals to probe on this project. To verify task switching behavior, use
CubeIDE's FreeRTOS kernel awareness view (Tasks tab in Debug perspective) to observe
task states - Running, Ready, Blocked - as the scheduler switches between them.
