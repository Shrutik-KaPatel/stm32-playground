# FreeRTOS_Semaphore

Binary semaphore demo. One task signals another using a semaphore - no data transferred,
just a notification. Demonstrates blocking, signaling pattern, and the difference between
semaphores and queues.

## What it does

- senderTask toggles PD12 (green) every 500ms then releases the semaphore
- receiverTask blocks on semaphore acquire, toggles PD13 (orange) when signaled
- Green and orange alternate - never simultaneously on due to context switch timing

## Key config decisions

- Binary semaphore initial state: Depleted. If set to Available, receiverTask runs
  through acquire immediately on startup without waiting for senderTask. Must start
  empty so receiver blocks until first signal.
- No queue needed - no data to transfer, only a signal. Semaphore is lighter and
  communicates intent more clearly than a queue with a dummy value.

## Execution flow

1. Scheduler starts, receiverTask hits osSemaphoreAcquire - counter 0, blocks
2. senderTask runs - toggles green ON, osDelay(500), releases semaphore
3. FreeRTOS wakes receiverTask immediately on release
4. senderTask loops back - toggles green OFF before receiverTask gets CPU
5. receiverTask runs - toggles orange ON, loops back, blocks again
6. Repeat

> [!NOTE]
> Green and orange never appear on simultaneously even though the intent seems like
> they should overlap. senderTask loops back and toggles green OFF before the
> scheduler switches to receiverTask. Context switches are not instantaneous from
> the task's perspective.

## Gotchas

> [!WARNING]
> HAL_GPIO_TogglePin just flips current state - it does not explicitly set ON or OFF.
> If you need precise control over when each LED turns on and off, use
> HAL_GPIO_WritePin with GPIO_PIN_SET and GPIO_PIN_RESET instead.

> [!WARNING]
> Binary semaphore has no ownership. Nobody needs to acquire it before releasing.
> osSemaphoreRelease from a task that never called osSemaphoreAcquire is perfectly
> valid. This is different from a mutex.

> [!IMPORTANT]
> osSemaphoreRelease does not just set a flag. FreeRTOS immediately checks if any
> task is blocked waiting on that semaphore and wakes it. The woken task does not
> run instantly - it becomes Ready and waits for the scheduler to give it CPU time.

## Semaphore vs queue vs mutex

- Queue: transfer data between tasks with buffering
- Binary semaphore: signal that something happened, no data, no ownership
- Mutex: protect a shared resource, only the acquirer can release it

> [!TIP]
> If you find yourself putting a dummy value in a queue just to signal another task,
> use a semaphore instead. The intent is clearer and the overhead is lower.

## Lessons Learned

- A binary semaphore is a counter capped at 1. Release increments it, acquire
  decrements it. Blocks when already 0. Simple as that.
- Semaphores are ISR-safe by design. Standard pattern for waking a task from an
  interrupt without polling.
- Initial state of the semaphore matters. Depleted means the receiver blocks
  immediately on startup until the first release. Available means it runs through
  once without waiting.
- Toggle-based LED code hides timing bugs. Explicit WritePin calls make ON/OFF
  intent clear and avoid unexpected state flips on loop-back.

## Logic analyzer

No external signals to probe on this project. To observe semaphore state, use
CubeIDE Debug perspective - add taskSemHandle to Expressions view and watch the
count change between 0 and 1 as tasks signal each other.
