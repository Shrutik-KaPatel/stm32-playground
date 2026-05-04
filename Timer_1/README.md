# TIM2 LED Blink with UART Logging

Blink an LED using a hardware timer interrupt, with live tick logging over UART via `printf`.

## What It Does

- **TIM2** fires an interrupt every 500ms (2 Hz)
- On each interrupt, the blue LED (PD15) toggles
- The main loop prints a `Tick N` message every second over UART2

## How It Works

| Component | Role |
|-----------|------|
| TIM2 | Hardware timer — triggers interrupt at 2 Hz |
| `HAL_TIM_PeriodElapsedCallback` | ISR — toggles LED on every timer overflow |
| UART2 (PA2/PA3) | Serial output for `printf` via `_write` override |
| `HAL_GetTick()` | Software millisecond counter for tick logging |

## Timer Math

Clock source: HSI (16 MHz), no PLL

```
16,000,000 / (7999 + 1) / (999 + 1) = 2 Hz → 500ms per toggle
```

## Key Code

```c
// Start timer with interrupts
HAL_TIM_Base_Start_IT(&htim2);

// Called automatically every 500ms by HAL
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
}
```

## Hardware

- **Board:** STM32F4DISCOVERY (MB997-F407VGT6-E01)
- **LED:** LD6 Blue → PD15
- **UART:** USART2 @ 115200 baud (PA2 TX, PA3 RX)

## Tools

- STM32CubeIDE 2.1.1 · STM32CubeMX 6.17.0 · HAL (STM32CubeF4)

## Lessons Learned

**1. Always define constants before using them**
Used `TICK_DELAY` in `main()` without defining it, causing a compile error: `'TICK_DELAY' undeclared`. Fixed by adding `#define TICK_DELAY 1000` in the `USER CODE BEGIN PD` section. Taught me to always define constants in the right place so CubeMX doesn't wipe them on code regeneration.

**2. Timer initialized ≠ Timer running**
The LED wasn't blinking even though the callback was written correctly. The bug was that `HAL_TIM_Base_Start_IT(&htim2)` was never called after `MX_TIM2_Init()`. CubeMX generates the init code but never starts the timer — that's always the developer's responsibility. A timer sitting initialized but not started will never fire an interrupt.

**3. Hz is always "per second"**
Learned to verify timer frequency using the formula:
`Timer Clock / (Prescaler + 1) / (Period + 1) = Hz`
And to convert to time period: `Period = 1 / Hz`. So 2 Hz = one toggle every 500ms, meaning the LED completes a full blink cycle every 1 second.
