# FreeRTOS_Watchdog

IWDG watchdog integration with FreeRTOS using a centralized watchdog task pattern.
Two worker tasks report alive status via flags. A dedicated high-priority watchdog
task kicks the IWDG only when all tasks have confirmed they are running. Any stuck
task causes the watchdog to stop kicking and the system to reset.

## What it does

- task1 and task2 do work, set alive flags every 500ms
- watchdogTask checks both flags every 1000ms
- If both flags set: kick IWDG, clear flags, print confirmation
- If any flag missing: print WARNING, do not kick
- After ~4 seconds without a kick, IWDG resets the board

## Why not kick from one task

In bare-metal, kicking the watchdog from the main loop works because there is only
one execution path. In RTOS, task1 could be happily kicking the watchdog while
task2 is completely deadlocked. The system appears healthy when it is not.

The centralized pattern requires proof of life from every task before kicking.
One stuck task = no kick = system reset. No task can cover for another.

> [!IMPORTANT]
> watchdogTask runs at osPriorityAboveNormal. If it ran at equal priority, a
> runaway task could starve it - the very situation it is supposed to detect.
> Higher priority guarantees the health check always runs.

## Execution flow

Normal operation:
Task1 running   (500ms)

Task2 running   (500ms)

Watchdog kicked (1000ms - both flags confirmed)

Task1 running

Task2 running

Watchdog kicked

...

Task2 stuck:
Task1 running

WARNING: task not responding  (task2Alive never set)

Task1 running

WARNING: task not responding

... (after ~4 seconds, IWDG fires, board resets)

> [!NOTE]
> The first message on startup is always WARNING. watchdogTask runs before task1
> and task2 have had a chance to set their first alive flags. This is expected
> behavior, not a bug. Normal operation begins from the second cycle onward.

## Alive flags

```c
volatile uint8_t task1Alive = 0;
volatile uint8_t task2Alive = 0;
```

Must be volatile - accessed by multiple tasks without mutex protection. Without
volatile the compiler may cache the value in a register and never see the update
from another task.

## Gotchas

> [!WARNING]
> The IWDG timeout must be significantly longer than the watchdog task check
> interval. Here: 4 second timeout, 1000ms check interval. If the timeout were
> 500ms and the check interval 1000ms, the watchdog would fire before the task
> even gets a chance to kick it.

> [!WARNING]
> Alive flags must be cleared by the watchdog task after checking, not by the
> worker tasks themselves. If worker tasks cleared their own flags, a task could
> set and clear its flag before watchdogTask checks - the check would always see 0.

> [!IMPORTANT]
> IWDG starts counting from HAL_IWDG_Init - before osKernelStart. The time between
> MX_IWDG_Init and the first kick in watchdogTask must be less than the timeout.
> At 4 seconds this is fine. For shorter timeouts, initialize IWDG after
> osKernelStart or add an early kick in USER CODE BEGIN 2.

## Key config decisions

- IWDG prescaler 32, reload 4095 → ~4 second timeout using LSI ~32kHz clock
- watchdogTask delay 1000ms - checks every second, well within 4 second timeout
- Worker task delay 500ms - report alive twice per watchdog check cycle
- osPriorityAboveNormal for watchdogTask - cannot be starved by normal tasks

## Scaling to more tasks

Adding a third task requires:
1. Add `volatile uint8_t task3Alive = 0`
2. Set `task3Alive = 1` in the new task loop
3. Add `task3Alive` to the watchdog if condition
4. Add `task3Alive = 0` to the clear block

Nothing else changes. Pattern scales linearly.

## Lessons Learned

- Unconditional watchdog kicking in RTOS is worse than no watchdog - it gives
  false confidence that the system is healthy.
- The alive flag pattern is simple and reliable. Each task owns its flag. The
  watchdog task owns the kick decision.
- volatile is not a substitute for a mutex - it only prevents compiler optimization.
  For this use case (one writer per flag, one reader) volatile is sufficient.
- IWDG cannot be stopped once started. If your debug session pauses at a breakpoint
  for more than 4 seconds, the board resets. Disable IWDG during active debugging
  or use WWDG which can be paused by the debug interface.

## Logic analyzer

No external signals critical to probe. To observe the reset behavior, probe
NRST pin - it will pulse low when IWDG fires after a stuck task experiment.
Confirms the reset is hardware-driven, not a software crash.
