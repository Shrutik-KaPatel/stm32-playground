# TIM_Interrupt

TIM2 fires an update interrupt every 500ms, toggling the green onboard LED (PD12) inside the callback. No HAL_Delay anywhere — CPU is free between interrupts.

---

## What it does

- TIM2 configured to overflow every 500ms
- Update interrupt fires on every overflow
- `HAL_TIM_PeriodElapsedCallback` toggles PD12 on each interrupt
- UART prints "Timer started" once at startup to confirm execution

---

## Key config decisions

**Timer:** TIM2 on APB1, internal clock source. Basic timer — no PWM, no channels needed, just counting and interrupting.

**PSC = 15999, ARR = 499:**
- 16 MHz ÷ (15999+1) = 1 kHz counter clock
- 1 kHz ÷ (499+1) = 2 Hz overflow = **500ms interval**

**NVIC enabled:** TIM2 global interrupt must be enabled in CubeMX. Without it the callback never fires — timer counts silently with no interrupt generated.

**Pin labelled LED in CubeMX:** PD12 labelled as LED so CubeMX generates `LED_GPIO_Port` and `LED_Pin` macros automatically. Cleaner than hardcoding GPIOD and GPIO_PIN_12 everywhere.

---

## Gotchas

- Always `Ctrl+S` in CubeIDE before switching to CubeMX to regenerate. If you don't save first, regeneration overwrites your unsaved edits and Replace editor content wipes them permanently.
- `HAL_TIM_PeriodElapsedCallback` fires for every timer interrupt on the chip — always check `htim->Instance` inside it or every timer will trigger your code.

---

## Lessons learned

**Timer interrupt vs HAL_Delay — fundamentally different.**
HAL_Delay blocks the CPU in a polling loop. A timer interrupt fires in the background and calls your callback — CPU does nothing in between. For anything beyond trivial single-task blink programs, timer interrupts are the right tool.

**HAL passes the handle, not the interrupt.**
`htim` in the callback is a pointer HAL passes to tell you which timer fired. The interrupt itself already happened by the time your callback runs. The `if` check is filtering which timer you care about, not catching the interrupt.

**`HAL_TIM_Base_Start_IT` is the on switch.**
Without this call the timer is configured but never starts. The `_IT` suffix enables interrupt mode — without it the timer counts but never fires the callback.
