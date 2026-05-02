# Flexible_Callback

A beginner embedded systems project for the **STM32F4DISCOVERY** board demonstrating the flexible callback pattern — using typedefs, structs, and a registration function to create a clean, scalable, and decoupled callback system.

---

## What It Does

When the user button **B1** is pressed, the green LED **LD4** toggles. The callback system is fully flexible — the ISR never knows which function it's calling, making the architecture easily extensible.

---

## Hardware

| Component | Pin |
|---|---|
| User Button B1 | PA0 |
| Green LED LD4 | PD12 |

---

## Core Concepts

### 1. typedef for Status Type
Creates a meaningful, readable name for the timer status:

```c
typedef uint32_t TIM_TIMER_STATUS;
```

Same as `uint32_t` underneath — but now the code tells a story!

### 2. typedef for Function Pointer
Creates a clean nickname for the ugly function pointer syntax:

```c
// Without typedef — ugly!
void (*callback)(TIM_TIMER_STATUS status, uintptr_t context);

// With typedef — clean and reusable!
typedef void (*TIM_TIMER_CALLBACK)(TIM_TIMER_STATUS status, uintptr_t context);
```

### 3. Struct — Bundling Callback + Context
Groups the function pointer and context data into one neat package:

```c
typedef struct
{
    TIM_TIMER_CALLBACK callback;  // the function pointer
    uintptr_t context;            // extra data if needed
} TIM_TIMER_CALLBACK_OBJ;

TIM_TIMER_CALLBACK_OBJ TIM3_CallbackObject;
```

Think of it like a business card — name (callback) and phone number (context) together!

### 4. Registration Function
Fills in the struct cleanly — hides internal details from the caller:

```c
void TIM3_TimerCallbackRegister(TIM_TIMER_CALLBACK callback, uintptr_t context)
{
    TIM3_CallbackObject.callback = callback;
    TIM3_CallbackObject.context = context;
}
```

Three benefits:
- **Readability** — clear what you're doing
- **Reusability** — call it anywhere to swap callbacks
- **Encapsulation** — caller never touches the struct directly

### 5. callback_execution — The Registered Function
Receives status and context, checks button, toggles LED:

```c
void callback_execution(TIM_TIMER_STATUS status, uintptr_t context)
{
    if(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_SET)
    {
        led_toggle_function();
    }
}
```

Status and context are available for future use — e.g. checking error conditions before proceeding.

### 6. ISR calls through the struct
The ISR never knows which function it's calling — pure flexibility:

```c
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if(htim == &htim3)
    {
        if(TIM3_CallbackObject.callback != NULL)
        {
            TIM3_CallbackObject.callback(0, TIM3_CallbackObject.context);
        }
    }
}
```

NULL check prevents crash if no function is registered yet.

---

## Program Flow

```
main() registers callback_execution into TIM3_CallbackObject
    → TIM3 fires every 10ms
        → HAL calls HAL_TIM_PeriodElapsedCallback
            → ISR checks NULL safety
                → calls TIM3_CallbackObject.callback(status, context)
                    → callback_execution runs
                        → button check (PA0)
                            → led_toggle_function()
                                → LED toggles (PD12)
```

---

## Comparison with all four callback projects

| | Button_Callback | Callback_Return | Hardware_Callback | Flexible_Callback |
|---|---|---|---|---|
| Callback return type | `void` | `unsigned char` | `void` | `void` |
| Callback parameter | none | none | `GPIO_PinState` | `status + context` |
| Function pointer | raw | raw | raw | typedef'd |
| Struct used | no | no | no | yes |
| Registration function | no | no | no | yes |
| ISR knows callback name | yes | yes | yes | no |
| Flexibility | good | better | better | best |

---

## Why uintptr_t for context?

`uintptr_t` is guaranteed to be large enough to hold a pointer address on any platform:
- STM32 (32-bit) → 32-bit context
- PC (64-bit) → 64-bit context

This makes the pattern **portable** across different architectures.

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
