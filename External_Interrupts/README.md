# External_Interrupts

An STM32F4DISCOVERY project demonstrating external interrupt handling, non-blocking LED blinking with variable speed, and UART printf output — all running concurrently without using `HAL_Delay()`.

## What It Does

- Blinks the onboard LED (PD15) using a non-blocking timer
- Each press of the user button (PA0) cycles the blink speed through three stages: **500ms → 250ms → 50ms → 500ms...**
- Prints a `Tick` count over UART every second
- Prints a message over UART on every button press

## Hardware

- **Board:** STM32F4DISCOVERY (MB997-F407VGT6-E01)
- **MCU:** STM32F407VGT6
- **LED:** LD6 Blue → PD15
- **Button:** B1 User → PA0

## Key Concepts

### Non-blocking timing
Instead of `HAL_Delay()` (which blocks everything), this project uses `HAL_GetTick()` with timestamp comparison:
```c
uint32_t now = HAL_GetTick();
if (now - last_blink >= blink_delays[blink_delay]) { ... }
```
This keeps the main loop free to handle other tasks simultaneously.

### Variable blink speed via button
Three blink intervals are stored in an array. Each button press advances the index, wrapping back to the start:
```c
uint16_t blink_delays[] = { 500, 250, 50 };

++blink_delay;
if (blink_delay >= sizeof(blink_delays) / sizeof(blink_delays[0])) blink_delay = 0;
```

### External Interrupt (EXTI)
The user button on PA0 is configured as a rising-edge interrupt. The ISR sets a flag, and the main loop acts on it — keeping ISR code minimal:
```c
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    if (GPIO_Pin == BTN_Pin) {
        BTN_press = 1;
    }
}
```

### printf over UART
`printf` is redirected to USART2 by implementing the `_write` syscall, transmitting via `HAL_UART_Transmit()`.

## Lessons Learned

- Variables shared between an ISR and the main loop **must be `volatile`**, otherwise the compiler may cache the value and never see the ISR's update
- The Discovery board's B1 button has an **external pull-down resistor** on PA0 — configuring an internal pull-up fights the hardware and prevents a clean rising edge, so `GPIO_NOPULL` must be used
- `HAL_GetTick()` returns `uint32_t` — storing it in a smaller type like `uint8_t` causes silent overflow and breaks timing logic
- Use `sizeof(array) / sizeof(array[0])` to get array length in C — avoids hardcoding the count and makes adding new speed steps trivial

## Project Structure

```
External_Interrupts/
├── Core/
│   ├── Inc/
│   │   └── main.h              # Pin definitions (BTN_Pin, LED_Pin, etc.)
│   └── Src/
│       ├── main.c              # Application logic
│       ├── stm32f4xx_it.c      # IRQ handlers (EXTI0_IRQHandler)
│       └── system_stm32f4xx.c
├── Drivers/                    # STM32 HAL drivers
└── External_Interrupts.ioc     # CubeMX configuration
```
