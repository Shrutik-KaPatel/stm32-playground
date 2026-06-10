# FreeRTOS_Mutex

Mutex demo protecting a shared UART resource. Two tasks compete to print messages
over UART. Shows race condition corruption without mutex, starvation without yield,
and clean fair access with both mutex and yield.

## What it does

- uartTask1 and uartTask2 both print to UART continuously via printf
- uartMutex ensures only one task transmits at a time
- PD12 and PD13 toggle on each print to show task activity on LEDs
- Output monitored via minicom at 115200 baud on /dev/ttyUSB0

## Hardware

- Waveshare FT232RNL USB-to-TTL adapter
- PA2 (USART2_TX) → adapter RXD
- PA3 (USART2_RX) → adapter TXD
- GND → GND

## printf redirect

```c
int _write(int file, char *ptr, int len)
{
    HAL_UART_Transmit(&huart2, (uint8_t*)ptr, len, HAL_MAX_DELAY);
    return len;
}
```

Place in USER CODE BEGIN 4. Routes all printf calls directly to UART2.
No manual HAL_UART_Transmit calls needed in task code.

## Three experiment phases

**Phase 1 - No mutex, no delay**
Both tasks hammer UART simultaneously. Output is garbled - chopped messages,
missing prefixes, interleaved bytes. Classic race condition on a shared peripheral.

**Phase 2 - Mutex, no delay**
Output is clean but only Task2 prints. Task2 releases the mutex and immediately
re-acquires it before Task1 gets scheduled. This is starvation.

**Phase 3 - Mutex with osDelay(10) after release**
Clean alternating output. Task releases mutex, yields for 10ms, other task acquires.
Fair access guaranteed.

> [!NOTE]
> osDelay after release is not always needed in production. If tasks have different
> priorities or natural delays from other work, starvation won't occur. Add it when
> two equal-priority tasks are competing for the same resource with no other delays.

## Gotchas

> [!WARNING]
> Mutex starvation is not obvious - output looks correct (no corruption) but one
> task is completely blocked. Always verify both tasks are running, not just that
> output is clean.

> [!WARNING]
> A mutex must be released by the same task that acquired it. Never release a mutex
> from a different task or an ISR - use a binary semaphore for that pattern instead.

> [!IMPORTANT]
> If you get build error `bad instruction 'ldr sp,=_estack'` in the startup file,
> open `Core/Startup/startup_stm32f407vgtx.s` and remove the corrupt prefix
> character on the ldr line. CubeIDE bug on Linux.

## Key config decisions

- USART2 asynchronous 115200 baud. PA2 TX, PA3 RX - standard USART2 pins on F407.
- One mutex: uartMutex. Protects the entire printf call including HAL_UART_Transmit
  inside _write. The whole transmission is the critical section.
- osDelay(10) after release - enough to yield to the other task without slowing
  output noticeably.

## Lessons Learned

- Race conditions on UART are immediately visible as garbled output. On other shared
  resources like flash memory or a sensor bus, corruption would be silent and
  catastrophic.
- Mutex ownership matters. Only the acquiring task releases it. This prevents one
  task from accidentally unlocking a resource another task is still using.
- Starvation can happen even with equal priority tasks. A task that never yields
  after releasing can re-acquire before the scheduler switches. Always add a yield
  after releasing in tight loops.
- The printf redirect pattern is reusable across all RTOS projects. Define _write
  once and printf works everywhere.

## Logic analyzer

Probe PA2 (USART2_TX) to observe UART frames. At 115200 baud each bit is ~8.7us.
Without mutex you will see overlapping start bits from both tasks. With mutex each
frame is clean and complete before the next begins.
