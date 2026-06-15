# FreeRTOS_DeferredISR

Deferred interrupt processing demo. Button press triggers an EXTI interrupt which
signals a FreeRTOS task via semaphore. All processing happens in task context, not
in the ISR. Demonstrates the standard RTOS pattern for interrupt-driven work.

## What it does

- B1 user button (PA0) configured as rising edge EXTI interrupt
- ISR callback releases isrSem immediately and returns
- handlerTask blocks on isrSem, wakes on each button press
- Toggles PD12 green LED and prints confirmation over UART
- Monitored via minicom at 115200 baud

## Why defer from ISR to task

ISRs must be short. In an RTOS context, most API functions are forbidden in ISR
context - printf, HAL_UART_Transmit, any blocking call will crash or corrupt the
system. Only a small set of ISR-safe functions exist.

Deferring moves all real work to task context where the full API is available,
timing is controlled by the scheduler, and the ISR returns immediately.

> [!IMPORTANT]
> NVIC interrupt priority must be 5 or higher (numerically) for any interrupt that
> calls FreeRTOS API functions. Priorities 0-4 are above the FreeRTOS kernel and
> cannot safely use any RTOS calls. CubeMX sets this automatically when FreeRTOS
> is enabled - do not lower it manually.

## The pattern
Button press → EXTI fires

→ HAL_GPIO_EXTI_Callback (one line, returns immediately)

→ osSemaphoreRelease (ISR-safe, no blocking)

→ handlerTask unblocks from osSemaphoreAcquire

→ toggles LED, prints to UART

→ blocks again, waiting for next press

## ISR callback

```c
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if(GPIO_Pin == GPIO_PIN_0)
    {
        osSemaphoreRelease(isrSemHandle);
    }
}
```

The callback runs once per button press - it is not a loop. Hardware re-triggers
it on each rising edge. It does one thing and returns.

## handlerTask

```c
for(;;)
{
    osSemaphoreAcquire(isrSemHandle, osWaitForever);
    HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_12);
    printf("Button pressed - handled in task context\r\n");
}
```

The task loops forever. Each iteration blocks until the ISR releases the semaphore.
One button press = one release = one wake = one processing cycle.

> [!NOTE]
> The callback and handlerTask are a pair. Hardware drives the callback, the callback
> drives the task. Neither works without the other.

## Gotchas

> [!WARNING]
> In CubeMX, always select USART2 in Asynchronous mode. Synchronous mode generates
> USART_HandleTypeDef husart2 and HAL_USART_Init - the printf redirect needs
> UART_HandleTypeDef huart2 from HAL_UART_Init. Wrong mode = build error on _write.

> [!WARNING]
> osSemaphoreRelease is ISR-safe. osSemaphoreAcquire with osWaitForever from an ISR
> will crash the system - ISRs cannot block. Never acquire with a timeout from ISR.

> [!WARNING]
> SYS Debug must be set to Serial Wire in CubeMX. Leaving it as Disable locks out
> ST-LINK after first flash - board becomes unresponsive to further programming.

## Key config decisions

- isrSem initial state Depleted. handlerTask blocks immediately at startup and waits
  for the first button press. If Available, task would run once on startup with no
  button press - wrong behavior.
- Rising edge trigger on PA0. B1 button on the F407 pulls PA0 high when pressed.
  Rising edge fires on press, not release.
- EXTI priority 5. Minimum safe priority for FreeRTOS API calls from ISR context.

## Lessons Learned

- The ISR is not a loop. It executes once per hardware event and returns. The
  hardware re-arms it automatically for the next event.
- Deferred processing is the correct RTOS pattern for any interrupt needing more
  than a flag set or semaphore release. printf in an ISR is never acceptable.
- The semaphore is a bridge between two execution contexts - interrupt and task.
  This bridge pattern appears everywhere in real firmware: UART RX complete,
  DMA transfer done, ADC conversion ready.
- FreeRTOS interrupt priority rules are non-negotiable. Priority too low = missed
  interrupts. Priority too high (0-4) = system crash when calling RTOS API.

## Logic analyzer

Probe PA0 to observe button press signal. Probe PA2 (USART2_TX) to verify UART
transmission happens after the rising edge on PA0, not during it. The gap between
PA0 rising and first UART bit confirms the deferred processing - work happens in
task context after the ISR has already returned.
