# PWM_InputCapture

TIM4 generates 1kHz PWM at 30% duty cycle on PD12. TIM3 measures it via input capture on PA6. A jumper wire connects PD12 to PA6. Results printed over UART every cycle — confirmed Pulse=300, Duty=30%.

---

## What it does

- TIM4 CH1 outputs PWM on PD12 (green LED LD4 visibly at 30% brightness)
- TIM3 CH1 reads the signal on PA6 via input capture
- Callback timestamps rising and falling edges, calculates pulse width and duty cycle
- Prints results continuously over UART2 at 115200 baud

---

## Hardware

Jumper wire: **PD12 → PA6** on extension headers P1/P2.

---

## Key config decisions

**Both timers PSC=15:** TIM3 must run at the same counter speed as TIM4 to measure ticks accurately. Both on APB1 at 16MHz → PSC=15 → 1MHz counter clock → 1 tick = 1 microsecond.

**TIM3 ARR=65535:** Max value. TIM3 is just counting — we don't want it overflowing mid-measurement and corrupting timestamps.

**TIM4 ARR=999, CCR=300:** 1kHz PWM at 30% duty cycle. CCR/ARR = 300/1000 = 30%.

**Input capture direct mode:** TIM3 CH1 configured as input, not output. Timestamps counter value on every detected edge.

---

## How the measurement works

Input capture fires a callback on every edge. We alternate between rising and falling edge detection:

1. Rising edge → timestamp saved as `rising_edge`, switch TIM3 to falling edge mode
2. Falling edge → timestamp saved as `falling_edge`, calculate pulse width
3. Switch back to rising edge mode, repeat

```
Signal:    _____|‾‾‾‾‾‾‾‾‾‾|_______
Callback:       ↑           ↓
                rising      falling
TIM3 mode: RISING → FALLING → RISING → ...

pulse_width = falling_edge - rising_edge
duty = (pulse_width * 100) / 1000
```

**Why polarity switching needs `__HAL_TIM_CLEAR_FLAG`:**
When you switch polarity, the signal may already be at the new level causing an immediate spurious capture event before the callback returns. This sets the capture flag again instantly, firing the callback with wrong data. Clearing the flag after every polarity switch discards that spurious trigger and waits for the next real edge.

Without the flag clear: Pulse=1000, Duty=100% (measuring rising→rising instead of rising→falling).
With the flag clear: Pulse=300, Duty=30% ✓

---

## Gotchas

- TIM3 and TIM4 must use the same PSC so tick counts are comparable. Different prescalers = wrong measurements silently.
- `falling_edge > rising_edge` guard added to discard measurements where counter wrapped — prevents garbage values from printing.

---

## Lessons learned

**Input capture measures time between edges, not voltage.**
ADC measures voltage. Input capture timestamps counter values at signal transitions. Pulse width = difference between rising and falling timestamps. No analog reading involved.

**Polarity must be switched dynamically to catch both edges.**
TIM3 can only listen for one edge type at a time. After catching the rising edge, switch to falling edge mode immediately. After catching the falling edge, switch back. Without this you only ever see rising→rising intervals.

**Spurious capture events happen on polarity switch.**
The moment you switch polarity the hardware may immediately detect the current signal level as a new edge and set the capture flag. Always call `__HAL_TIM_CLEAR_FLAG(htim, TIM_FLAG_CC1)` after switching to discard it.

**PWM generate + measure is a self-contained test.**
No external hardware needed — one timer generates, another measures, a jumper wire connects them. Useful for verifying both timer configurations are correct before connecting real sensors.
