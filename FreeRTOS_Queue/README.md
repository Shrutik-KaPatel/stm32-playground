# FreeRTOS_Queue

Producer/consumer demo using a FreeRTOS message queue. One task generates data and
sends it through a queue, another task receives and reacts. Demonstrates thread-safe
inter-task communication and blocking behavior without polling.

## What it does

- producerTask increments a counter every 500ms, puts it in the queue, toggles PD12 (green)
- consumerTask blocks on the queue waiting for data, toggles PD13 (orange) on each receive
- Both LEDs blink in sync - consumer reacts instantly to every send

## Execution flow

1. producerTask increments counter, puts value in queue, toggles green, osDelay(500)
2. Producer blocks - scheduler switches to consumerTask
3. consumerTask was blocked on osMessageQueueGet - data arrived, wakes immediately
4. consumerTask toggles orange, loops back, blocks again on empty queue
5. 500ms later producer wakes, repeats

> [!NOTE]
> Consumer timing is driven entirely by the queue. Consumer has no osDelay - it just
> reacts the moment data arrives. Think of it as a doorbell - no concept of time,
> just responds to data.

## Key config decisions

- Queue size 5, item size uint32_t. Size is a design choice - too small stalls the
  producer, too large wastes RAM.
- osWaitForever on both put and get. Producer blocks if queue full, consumer blocks
  if queue empty. No data dropped, no polling.
- Consumer uses a local `received` variable, not the producer's `counter`. Queue
  copies the value safely across task boundaries.

## Backpressure experiment

Added osDelay(2000) to consumer to simulate a slow processor. After 5 items the
queue fills and producer blocks on osMessageQueuePut - green LED visibly pauses
until consumer makes space. No data lost.

> [!TIP]
> Queue size is a real design decision in production firmware. Too small and fast
> producers stall. Too large and you waste precious RAM on an embedded target.

## Gotchas

> [!WARNING]
> Do not share the producer's variable with the consumer for receiving. Use a
> separate local variable. The queue copies the value - both tasks do not need
> to touch the same memory.

> [!WARNING]
> osMessageQueueGet blocks the calling task, not the CPU. Other tasks run while
> consumer waits. This is not the same as a polling loop.

> [!IMPORTANT]
> Queue must be created via osMessageQueueNew before osKernelStart. CubeMX
> handles this ordering automatically - do not move queue creation into task
> functions.

## Lessons Learned

- A queue decouples producer and consumer completely. Neither task needs to know
  about the other's timing or existence.
- Race conditions from shared globals are eliminated - the queue is thread-safe
  by design. The RTOS protects every put and get internally.
- Blocking on an empty queue uses zero CPU. FreeRTOS puts the task to sleep and
  runs other tasks. The bare-metal equivalent would be a polling loop burning cycles.

## Logic analyzer

No external signals to probe on this project. To observe queue behavior, use
CubeIDE Debug perspective - add dataQueueHandle to the Expressions view and watch
the queue count change as producer sends and consumer receives.
