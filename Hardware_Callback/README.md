# Hardware_Callback

A beginner embedded systems project for the **STM32F4DISCOVERY** board demonstrating the hardware callback pattern — where application code calls back into the HAL driver to control hardware, keeping the ISR decoupled from hardware details.

---

## What It Does

When the user button **B1** is held down, the green LED **LD4** turns ON. When the button is released, the LED turns OFF. Same behavior as `Callback_Return` but with a cleaner, more decoupled architecture.

---

## Hardware

| Component | Pin |
|---|---|
| User Button B1 | PA0 |
| Green LED LD4 | PD12 |

---

## Core Concepts

### 1. Hardware Callback Pattern
A two-way conversation between your application code and the HAL driver:

```
HAL driver fires (TIM3 event)
    → calls your application function
        → your function reads button
            → calls hardwareCallback with the state
                → hardwareCallback calls HAL to control LED
```

HAL is always the middleman — your code never touches hardware registers directly.

### 2. Callback with Parameter
Unlike previous projects, this callback **receives a parameter** that tells it what to do:

```c
// Project 2 — no parameter, no return
void (*callbackFunction)(void)

// Project 3 — no parameter, returns value
unsigned char (*functionPointer)(void)

// This project — takes a parameter, no return
void (*hardwareCallback)(GPIO_PinState)
```

`GPIO_PinState` is a HAL enum:
```c
typedef enum
{
    GPIO_PIN_RESET = 0,  // OFF
    GPIO_PIN_SET   = 1   // ON
} GPIO_PinState;
```

### 3. ledControl — the hardware callback
Receives the desired LED state and calls HAL to apply it:

```c
void ledControl(GPIO_PinState state)
{
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_12, state);
}

void (*hardwareCallback)(GPIO_PinState) = &ledControl;
```

### 4. ISR has zero hardware knowledge
The ISR reads the button and passes the decision to the callback — it doesn't know or care which LED, port, or pin is involved:

```c
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if(htim == &htim3)
    {
        if(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_SET)
            hardwareCallback(GPIO_PIN_SET);    // tell callback: ON
        else
            hardwareCallback(GPIO_PIN_RESET);  // tell callback: OFF
    }
}
```

---

## Program Flow

```
TIM3 fires every 10ms
    → HAL_TIM_PeriodElapsedCallback (HAL calls us)
        → read button state (PA0)
            → call hardwareCallback with state
                → ledControl calls HAL_GPIO_WritePin (we call HAL back)
                    → HAL controls physical pin PD12
```

---

## Comparison with all three callback projects

| | Button_Callback | Callback_Return | Hardware_Callback |
|---|---|---|---|
| Callback return type | `void` | `unsigned char` | `void` |
| Callback parameter | none | none | `GPIO_PinState` |
| Hardware control | inside callback | inside ISR | via HAL through callback |
| ISR knows hardware? | yes | yes | no |
| Coupling | medium | medium | loosest |
| Flexibility | good | better | best |

---

## Why This Pattern is Best

Changing the LED pin only requires touching **one function**:

```c
void ledControl(GPIO_PinState state)
{
    // change only here — nothing else needs updating!
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_15, state);  // now uses blue LED
}
```

The ISR, the function pointer, and the rest of the program remain completely unchanged.

---

## A Note on Performance

The constant back-and-forth between application code and HAL has negligible cost because:
- HAL functions are thin wrappers around single register writes
- This only happens 100 times per second (every 10ms)
- At 16MHz, each callback has ~160,000 instructions available

Only optimize when you measure a real performance problem — not before.

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
