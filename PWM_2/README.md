# PWM_2 — Breathing LED via TIM2 PWM on PA0

## What this does

Fades the blue LED (PD15) in and out smoothly — the classic "breathing" effect — by
continuously adjusting the PWM duty cycle at runtime. Every 10ms, the compare value
steps up or down by 1, sweeping from 0 to 1000 and back. At 100 Hz PWM frequency and
1000 steps, this gives a full fade cycle of about 20 seconds (10ms × 1000 steps × 2
directions). Still using the PA0 → PD15 jumper wire from PWM_1.

---

## Hardware setup

Same as PWM_1 — jumper wire from **PA0 → PD15**. No changes needed on the hardware
side. PA0 is TIM2_CH1's output pin, PD15 is the blue LED floating in input mode, wire
bridges them.

---

## How the breathing works

The key is `__HAL_TIM_SET_COMPARE` — a macro that writes directly to the timer's CCR
(Capture/Compare Register) while the timer is already running. This changes the duty cycle
on the fly without stopping or reinitializing the timer. The PWM frequency stays fixed at
100 Hz the whole time; only the pulse width changes.

The fade logic in the while loop:

```c
if (now >= next_change) {
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, pwm_value);
    pwm_value += pwm_chnage;      // step up or down by 1

    if (pwm_value == 0)    pwm_chnage = 1;   // hit bottom, go up
    if (pwm_value == 1000) pwm_chnage = -1;  // hit top, go down

    next_change = now + 10;  // update every 10ms
}
```

`pwm_chnage` (yes, typo — it's in the code) acts as the direction flag: +1 when fading in,
-1 when fading out. Each iteration it adds itself to `pwm_value`, and flips sign when either
boundary is hit.

---

## Timer config (same as PWM_1)

```
TIM2 clock  = 84 MHz (APB1 × 2)
Prescaler   = 839   →  84,000,000 / 840 = 100,000 Hz tick rate
Period      = 999   →  100,000 / 1000   = 100 Hz PWM frequency
```

100 Hz is above the flicker threshold so the LED looks smooth, not choppy. Initial Pulse
set to 100 in CubeMX config but immediately overridden at runtime by `__HAL_TIM_SET_COMPARE`.

---

## Gotchas

**`_HAL_TIM_SET_COMPARE` vs `__HAL_TIM_SET_COMPARE`** — the macro has two leading
underscores, not one. One underscore → compiler error: implicit declaration of function.
Easy to miss since they look almost identical. Took a build failure to catch it.

**`pwm_value += pwm_value` instead of `pwm_value += pwm_chnage`** — the first attempt
accidentally doubled `pwm_value` each iteration instead of incrementing it by the step
direction. Starting from 0, doubling gives 0 forever — LED sat completely off and nothing
moved. Classic copy-paste brain error.

**`uint8_t` for `pwm_chnage`** — declared as unsigned but used as a signed direction
flag (-1 or +1). This works because -1 stored in a uint8_t wraps to 255, and 255 added
to a uint16_t effectively subtracts 1 only if the types align correctly. Got lucky here — should
really be `int8_t` to be explicit about the sign.

---

## Lessons learned

- `__HAL_TIM_SET_COMPARE` is the right way to change PWM duty cycle at runtime. It
  writes directly to the CCR register — no need to stop the timer, reconfigure, or restart.
  The change takes effect on the next PWM cycle automatically.

- The difference between PWM frequency and duty cycle is fully decoupled in hardware.
  Frequency is locked by Prescaler + Period. Duty cycle is just the CCR value, which you
  can change anytime. This is what makes real-time brightness control possible without
  touching the timer config.

- `uwTick` based non-blocking timing (checking `now >= next_change`) is the right pattern
  for doing multiple things at different rates in a single while loop. The tick printer runs
  every 1000ms, the PWM stepper runs every 10ms — both coexist without either blocking
  the other. No `HAL_Delay` anywhere.
