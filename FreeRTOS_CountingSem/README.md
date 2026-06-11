# FreeRTOS_CountingSem

Counting semaphore demo managing a pool of 2 slots shared between 3 worker tasks.
Shows how a counting semaphore limits concurrent access to a fixed number of
resources - only 2 tasks can "work" simultaneously, the third always blocks.

## What it does

- Three worker tasks each try to acquire a slot, simulate work with a 2s delay,
  release the slot, then wait 500ms before trying again
- Semaphore max count is 2 - only 2 tasks can hold a slot at any time
- Third task always blocks on acquire until one of the others releases
- Output via printf over USART2 shows acquire/release sequence in minicom

## Key config decisions

- Max count 2, initial count 2. All slots available at startup. If initial count
  were 0, every task would block immediately on startup - nothing would run.
- osDelay(2000) simulates work inside the slot. osDelay(500) after release gives
  other tasks a fair chance to acquire before looping back.
- USART2 printf redirect used for visibility - no mutex on UART here since the
  demo point is the semaphore, not UART protection.

> [!IMPORTANT]
> CubeMX may generate osSemaphoreNew(N, 0, ...) with initial count 0. Always
> verify the second argument matches the first. Initial count 0 means all slots
> taken at startup - every task blocks and nothing runs.

## Observed behavior

With 2 slots and 3 tasks, you never see 3 simultaneous "acquired" messages.
The pattern is always:
- 2 tasks acquire
- 1 task blocks
- One releases → blocked task acquires immediately
- Rotation continues

> [!NOTE]
> Think of it as a parking lot with 2 spaces and 3 cars. One car always waits.
> The semaphore count is the number of empty spaces.

## Gotchas

> [!WARNING]
> Always acquire before use, release after. Never release a slot you did not
> acquire - this increments the count above the maximum and corrupts the pool.

> [!WARNING]
> No mutex on UART in this project. Output may occasionally appear slightly
> interleaved. The demo point is the semaphore slot behavior, not UART protection.
> In production, protect UART with a mutex as shown in FreeRTOS_Mutex.

## Counting vs binary semaphore

- Binary semaphore: max count 1, used for signaling between tasks
- Counting semaphore: max count N, used for managing a pool of N resources
- Both use the same acquire/release API - only the max count differs

## Lessons Learned

- Counting semaphores are the natural fit for resource pools. Any time you have
  N identical resources and more than N consumers, a counting semaphore is the
  right tool.
- Initial count matters as much as max count. A semaphore starting at 0 is a
  valid pattern for signaling but wrong for resource pools.
- With N-1 slots and N tasks, one task is always blocked. This is visible in
  output as a rotation - useful for verifying the semaphore is actually working.
- Counting semaphores are less common in typical embedded firmware than binary
  semaphores and mutexes. Most resource sharing uses mutexes; counting semaphores
  appear in buffer pool managers and rate limiters.

## Logic analyzer

No external signals to probe. To observe semaphore count changing, add
slotSemHandle to the Expressions view in CubeIDE Debug perspective and watch
the count decrement on acquire and increment on release.
