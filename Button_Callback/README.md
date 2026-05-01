# Button_Callback

A beginner embedded systems project for the **STM32F4DISCOVERY** board demonstrating timer interrupts, software button debouncing, and the callback design pattern.

---

## What It Does

When the user button **B1** is pressed and held for at least 50ms, the green LED **LD4** toggles. The LED stays in its current state when the button is released.

---

## Hardware

| Component | Pin |
|---|---|
| User Button B1 | PA0 |
| Green LED LD4 | PD12 |

---

## Core Concepts

### 1. Timer Interrupt
TIM3 is configured to fire an interrupt every **10ms**. This creates a reliable time base for the rest of the program without blocking the main loop.

```
Clock: 16MHz
Prescaler: 1599
Period: 99
Interrupt frequency: 100Hz (every 10ms)
```

### 2. Software Debouncing
Mechanical buttons bounce when pressed, creating multiple rapid signals. To filter this out, we only confirm a press after the button has been held continuously for **50ms** (5 consecutive 10ms ticks).

```
Button held → counter increments every 10ms
Button released → counter resets to 0
Counter reaches 5 → real press confirmed!
```

### 3. Callback Pattern
Instead of hardcoding the action inside the interrupt, a **function pointer** is used. This means the behavior can be swapped out without changing the interrupt or debounce logic.

```c
void (*callbackFunction)(void) = &toggleLED;
```

Tomorrow, to change the behavior:
```c
callbackFunction = &playSound;   // no other code changes needed!
```

---

## Program Flow

```
TIM3 fires every 10ms
    → HAL_TIM_PeriodElapsedCallback runs
        → checks if B1 is pressed (PA0)
            → if pressed: increment debounceCounter
                → if counter >= 5: confirmed press!
                    → reset counter
                    → call callbackFunction()
                        → toggleLED() toggles PD12
            → if not pressed: reset debounceCounter to 0
```

---

## Project Structure

```
Button_Callback/
├── Core/
│   ├── Src/
│   │   └── main.c        # Timer callback, debounce, callback pattern
│   └── Inc/
│       └── main.h
├── Drivers/              # HAL drivers (auto-generated)
└── Button_Callback.ioc   # CubeMX configuration
```

---

## Tools Used

- **STM32CubeIDE** v2.1.1
- **STM32CubeMX** v6.17.0
- **HAL Library** (STM32CubeF4)

---

## Board

STM32F4DISCOVERY (MB997-F407VGT6-E01)
