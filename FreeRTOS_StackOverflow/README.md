# FreeRTOS_StackOverflow

Stack overflow detection demo using FreeRTOS Method 2 watermark checking.
Deliberately causes a stack overflow in one task, catches it in the hook with the
task name, then fixes it by increasing stack size. Shows the full detection and
resolution workflow.

## What it does

- normalTask prints "normalTask running" every 1000ms - healthy reference task
- overflowTask declares a 600-byte local array that exceeds its 512-byte stack
- vApplicationStackOverflowHook catches the overflow, prints which task caused it,
  halts the system
- Phase 2: stack increased to 512 words - both tasks run normally with no overflow

## Two phases

**Phase 1 - Overflow (128 word stack)**
overflowTask allocates uint8_t bigBuffer[600] as a local variable. This alone
exceeds the 512-byte stack. FreeRTOS Method 2 detects the corrupted watermark
pattern at the next context switch and calls the hook.

**Phase 2 - Fixed (512 word stack)**
Stack increased to 512 words (2048 bytes). The 600-byte array fits comfortably
with room for function call overhead and saved registers. Both tasks run
continuously with no overflow detected.

> [!NOTE]
> The UART output in Phase 2 appears slightly interleaved because there is no
> mutex protecting UART - both tasks print simultaneously. This is intentional;
> the focus of this project is stack detection, not UART protection. See
> FreeRTOS_Mutex for the correct UART sharing pattern.

## Stack overflow hook

The hook lives in freertos.c, not main.c. CubeMX generates a stub there
automatically. Defining it in main.c causes a multiple definition linker error.

```c
void vApplicationStackOverflowHook(xTaskHandle xTask, signed char *pcTaskName)
{
    char msg[] = "STACK OVERFLOW DETECTED\r\n";
    __HAL_RCC_GPIOD_CLK_ENABLE();
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_12, GPIO_PIN_SET);
    HAL_UART_Transmit(&huart2, (uint8_t*)msg, sizeof(msg)-1, HAL_MAX_DELAY);
    while(1);
}
```

HAL_UART_Transmit used directly instead of printf - avoids dependency on the
_write redirect in main.c. huart2 accessed via extern declaration in freertos.c.

## Gotchas

> [!WARNING]
> CubeMX may not update the .stack_size field in the task attributes struct when
> you change stack size in the GUI. Always verify main.c after regenerating:
> `.stack_size = 512 * 4` not `128 * 4`. Fix manually if needed.

> [!WARNING]
> vApplicationStackOverflowHook must be defined inside the existing stub in
> freertos.c. Defining it anywhere else causes a multiple definition linker error
> because CubeMX already declares the prototype there.

> [!IMPORTANT]
> Always set SYS Debug to Serial Wire in CubeMX. Leaving it as Disable locks out
> ST-LINK after first flash. No warning appears - board silently becomes
> unreprogrammable until rescue procedure.

> [!IMPORTANT]
> Enable CHECK_FOR_STACK_OVERFLOW during all development. Without it, a stack
> overflow produces a random HardFault with no useful information - nearly
> impossible to trace back to the actual cause.

## Key config decisions

- Method 2 chosen over Method 1. Method 1 only checks stack pointer position.
  Method 2 checks watermark pattern - catches overflows that happen and partially
  recover within one time slice. Slight performance cost is worth it during
  development.
- 512 word final stack size. 600 byte array plus ~200-300 bytes for function call
  overhead, saved registers, and FreeRTOS task context. 256 words was still not
  enough - overhead is larger than it looks.
- normalTask kept at 128 words. Simple task with no local arrays - default is
  sufficient. Stack size should be set per task based on actual usage.

## Stack sizing rule of thumb

Stack size = largest local variable or array + 200-300 bytes minimum overhead.
When uncertain, set large and use uxTaskGetStackHighWaterMark() to measure actual
peak usage, then reduce to fit with margin.

## Lessons Learned

- Stack overflow symptoms appear far from the cause. The corrupted memory may
  affect a completely different task or FreeRTOS internals - the crash looks
  random and unrelated to the actual overflow.
- The hook gives you the task name. This single piece of information cuts
  debugging time from hours to seconds.
- Local variables are the most common cause of overflow. Large arrays, deep
  function call chains, and recursion all consume stack silently.
- CubeMX default 128 words is a starting point only. Any task with significant
  local data or deep call chains needs a larger stack.
- Always enable overflow detection in development. The performance cost of
  Method 2 is negligible compared to the debugging cost of a silent overflow.

## Logic analyzer

No external signals to probe. To measure actual stack usage without triggering
overflow, use uxTaskGetStackHighWaterMark(NULL) inside the task and print the
result over UART. Returns the minimum free stack words remaining since task start.
