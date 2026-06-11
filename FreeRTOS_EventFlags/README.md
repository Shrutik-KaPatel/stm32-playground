# FreeRTOS_EventFlags

Event flags demo waiting for multiple independent conditions before taking action.
Three tasks set flags independently - a button press, a touch sensor, and a 3-second
timer. A fourth task blocks until all three flags are set simultaneously, then acts.

## What it does

- buttonTask polls onboard B1 (PA0) every 200ms, sets BUTTON_FLAG on press
- sensorTask polls TTP223B touch sensor (PA1) every 50ms, sets SENSOR_FLAG on touch
- timerTask sets TIMER_FLAG every 3 seconds automatically
- actionTask blocks until ALL three flags set, toggles PD12 green LED, prints confirmation
- All flags cleared automatically after actionTask unblocks - cycle repeats from scratch

## Hardware

- Onboard B1 user button: PA0 (no wiring needed)
- TTP223B touch sensor: VCC → 3.3V, GND → GND, SIG → PA1
- Onboard green LED: PD12
- Waveshare FT232RNL: PA2 (USART2_TX) → RXD, GND → GND

## Flag definitions

```c
#define BUTTON_FLAG   (1 << 0)   // 0x01
#define SENSOR_FLAG   (1 << 1)   // 0x02
#define TIMER_FLAG    (1 << 2)   // 0x04
```

Each flag is a unique bit in a 32-bit register. Always use 1 << N, never 0 << N.
Combine with OR to reference multiple flags in one call.

## Why event flags over semaphores

Three semaphores acquired sequentially would force a fixed order - wait for button,
then sensor, then timer. If the sensor fires before the button is pressed, that event
is missed by the time the task reaches the sensor acquire call.

Event flags solve this - all three conditions are monitored simultaneously regardless
of order. The task sleeps once and wakes only when the complete set is satisfied.

> [!NOTE]
> This maps directly to the DMA completion flag pattern. DMA runs independently and
> sets a flag when done. Event flags extend this to multiple independent operations
> each signaling completion - the waiting task reacts only when all are done.

## Execution flow

1. timerTask fires TIMER_FLAG every 3s automatically
2. buttonTask sets BUTTON_FLAG when B1 pressed (change detection)
3. sensorTask sets SENSOR_FLAG when touch detected (change detection)
4. actionTask wakes when all three bits set - order does not matter
5. Toggles PD12, prints "All conditions met\r\n"
6. osEventFlagsWait clears all three flags automatically
7. Repeat - timerTask resets in 3s, button and sensor need re-trigger

> [!TIP]
> To test osFlagsWaitAny behavior, change osFlagsWaitAll to osFlagsWaitAny.
> actionTask will then fire on the first of the three conditions met rather
> than waiting for all three.

## Gotchas

> [!WARNING]
> osEventFlagsWait clears flags automatically on unblock when using osFlagsWaitAll
> or osFlagsWaitAny. Do not manually clear afterward or you may clear flags that
> were set by other tasks between the wait returning and your clear call.

> [!WARNING]
> Only bits 0-23 are available for application use. The top 8 bits of the 32-bit
> event flags register are reserved by FreeRTOS internally. Define no more than
> 24 flags per event group.

> [!IMPORTANT]
> Setting tasks run freely and never block on osEventFlagsSet. Only the waiting
> task blocks. A flag set before the waiting task calls osEventFlagsWait is not
> lost - it stays set in the register until cleared.

## Key config decisions

- Change detection in buttonTask and sensorTask - only set flag on state transition,
  not every poll. Without this, flags get set repeatedly and actionTask could fire
  on stale repeated events.
- timerTask uses osDelay(3000) as a simple periodic timer. In production use an
  RTOS software timer instead of a dedicated task for periodic events.
- All tasks osPriorityNormal. No priority needed - actionTask spends almost all
  its time blocked, consuming zero CPU.

## Lessons Learned

- Event flags are the natural fit when multiple independent conditions must all be
  true before an action is taken. Semaphores chained in sequence cannot express
  this cleanly.
- osFlagsWaitAll vs osFlagsWaitAny gives AND vs OR logic on conditions. This single
  parameter change completely alters the system behavior.
- Flags persist in the register until cleared. A flag set while the waiting task is
  doing other work is not lost - it will be seen on the next osEventFlagsWait call.
- The pattern of dedicated setter tasks and one waiting task scales cleanly. Adding
  a fourth condition means adding one task, one flag define, and one bit to the wait
  mask - nothing else changes.

## Logic analyzer

No external signals critical to probe here. To observe flag register state, add
sysEventsHandle to the Expressions view in CubeIDE Debug perspective and watch
the 32-bit value change as each task sets its flag bit.
