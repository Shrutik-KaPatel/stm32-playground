# FreeRTOS_ConfigAssert

configASSERT demo showing how to replace the default silent halt with a diagnostic
handler that prints the exact file and line number of the assertion failure over UART.
Demonstrates the difference between an undebuggable crash and a clear diagnostic.

## What it does

- defaultTask starts, delays 1 second, then triggers configASSERT(0) deliberately
- Custom handler prints file name and line number to minicom before halting
- PD12 green LED turns on to give visual indication of assertion failure
- "Task running" never prints - system halted at the assertion point

## Output
Task started

ASSERT FAILED: ../Core/Src/main.c line 292

You know immediately: which file, which line, what happened. No random crash,
no hunting through code.

## Why configASSERT matters

The default configASSERT just disables interrupts and spins - identical to a
mystery crash. You know something went wrong but not what or where.

FreeRTOS uses configASSERT internally throughout the kernel to check:
- NULL handles passed to API functions
- Invalid priority levels
- Blocking API called from ISR context
- Stack pointer out of bounds

Without a custom handler all of these produce silent failures. With a custom
handler each one tells you exactly where the problem is.

> [!NOTE]
> configASSERT is a development tool only. Disable it in production builds to
> save code size and avoid halting on edge cases that production code should
> handle gracefully.

## configASSERT definition in FreeRTOSConfig.h

```c
/* USER CODE BEGIN 1 */
#define configASSERT( x ) if ((x) == 0) { configASSERT_Handler(__FILE__, __LINE__); }
/* USER CODE END 1 */
```

Also add extern declaration in FreeRTOSConfig.h USER CODE BEGIN Includes:

```c
#if defined(__ICCARM__) || defined(__CC_ARM) || defined(__GNUC__)
extern void configASSERT_Handler(const char *file, int line);
#endif
```

## Handler implementation

```c
void configASSERT_Handler(const char *file, int line)
{
    __disable_irq();
    printf("ASSERT FAILED: %s line %d\r\n", file, line);
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_12, GPIO_PIN_SET);
    while(1);
}
```

Place in USER CODE BEGIN 4 in main.c. Add prototype in USER CODE BEGIN PFP.

## Gotchas

> [!WARNING]
> Wrong macro syntax in FreeRTOSConfig.h causes 800+ cascade errors through all
> FreeRTOS source files. An extra brace or missing brace in the macro definition
> corrupts every file that includes FreeRTOS.h. Check the macro syntax carefully
> before building.

> [!WARNING]
> CMSIS-V2 wrapper does not always propagate configASSERT for NULL handle checks.
> osSemaphoreAcquire(NULL, ...) may silently fail rather than asserting. Use
> configASSERT(0) directly to test the handler works before relying on it.

> [!IMPORTANT]
> The extern declaration for configASSERT_Handler must be inside the compiler
> guard in FreeRTOSConfig.h. Without it the assembler sees the declaration and
> throws errors during startup file compilation.

## Key config decisions

- Custom handler over default. Default spins silently - useless for debugging.
  Custom handler costs a few bytes of code but saves hours of debugging time.
- printf in handler is safe here because UART is initialized before osKernelStart
  and the assert fires in task context. If assert could fire before UART init,
  use HAL_UART_Transmit directly with a hardcoded string instead.
- LED indicator alongside UART. If UART is not connected, LED still gives visual
  confirmation that an assertion fired.

## Lessons Learned

- configASSERT is the single most valuable debug tool in FreeRTOS development.
  Enable it on every project from the start.
- File and line number in the output is the key advantage. It turns a symptom
  into a cause immediately.
- FreeRTOS internal assertions catch API misuse before it causes corruption.
  A NULL handle caught by configASSERT at the call site is infinitely easier
  to fix than the corruption it would cause three function calls later.
- Always test your configASSERT handler works with configASSERT(0) before
  relying on it to catch real errors.

## Logic analyzer

No external signals to probe. To verify the assertion fires before any further
code executes, probe PA2 (USART2_TX) and confirm only two UART transmissions
occur - "Task started" and the assert message - with no further traffic.
