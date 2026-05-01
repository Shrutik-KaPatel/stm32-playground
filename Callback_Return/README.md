# Callback_Return

A beginner embedded systems project for the **STM32F4DISCOVERY** board demonstrating callbacks that return a value, used to directly control an LED based on button state.

---

## What It Does

When the user button **B1** is held down, the green LED **LD4** turns ON. When the button is released, the LED turns OFF.

---

## Hardware

| Component | Pin |
|---|---|
| User Button B1 | PA0 |
| Green LED LD4 | PD12 |

---

## Core Concepts

### 1. Callback with Return Value
Unlike the previous `Button_Callback` project where the callback was `void`, here the callback **returns a value** that the caller uses to make a decision.

```c
// Previous project — callback returns nothing
void (*callbackFunction)(void) = &toggleLED;

// This project — callback returns a result
unsigned char (*functionPointer)(void) = &pin_monitor;
```

### 2. pin_monitor — the callback function
Reads the button pin directly and returns:
- `1` = button is pressed
- `0` = button is not pressed

```c
unsigned char pin_monitor(void)
{
    if(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_SET)
        return 1;  // pressed
    else
        return 0;  // not pressed
}
```

### 3. callback_execution — the library gateway
Receives the function pointer, calls it, and passes the return value back to the caller. This is the single entry point — like a receptionist that controls access.

```c
unsigned char callback_execution(unsigned char(*fn)(void))
{
    return fn();
}
```

### 4. Return value controls behavior
The result from `callback_execution` directly decides the LED state:

```c
if(callback_execution(functionPointer))
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_12, GPIO_PIN_SET);   // LED ON
else
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_12, GPIO_PIN_RESET); // LED OFF
```

---

## Program Flow

```
TIM3 fires every 10ms
    → HAL_TIM_PeriodElapsedCallback runs
        → calls callback_execution(functionPointer)
            → callback_execution calls pin_monitor()
                → pin_monitor reads PA0
                    → returns 1 (pressed) or 0 (not pressed)
        → return value decides LED state
            → 1 → GPIO_PIN_SET  → LED ON
            → 0 → GPIO_PIN_RESET → LED OFF
```

---

## Comparison with Button_Callback project

| | Button_Callback | Callback_Return |
|---|---|---|
| Callback return type | `void` | `unsigned char` |
| Callback assigned | at runtime in `main()` | at declaration |
| LED behavior | toggles on confirmed press | ON while held, OFF when released |
| Debouncing | yes (50ms counter) | no (direct pin read) |
| Decision maker | callback itself | caller uses return value |

---

## Why No Debounce Here?

This project reads the button state continuously every 10ms and directly reflects it on the LED. Since the LED is not toggling but simply mirroring the button state, brief bounce spikes don't cause a noticeable problem — the LED just stabilizes within a few milliseconds.

Debouncing matters most when a single press must trigger exactly one action (like a toggle). Here, the action is continuous (hold = ON), so debounce is not critical.

---

## Timer Configuration

```
Clock: 16MHz
Prescaler: 1599
Period: 99
Interrupt frequency: 100Hz (every 10ms)
```

---

## Tools Used

- **STM32CubeIDE** v2.1.1
- **STM32CubeMX** v6.17.0
- **HAL Library** (STM32CubeF4)

---

## Board

STM32F4DISCOVERY (MB997-F407VGT6-E01)
