# PWM_LED

PWM dimming on all four onboard LEDs (PD12–PD15) using TIM4. LEDs breathe smoothly from off to full brightness and back, looping forever. CPU does zero work during the fade — PWM runs entirely in hardware.

---

## What it does

- TIM4 generates 1 kHz PWM on all four channels simultaneously
- `__HAL_TIM_SET_COMPARE` updates CCR at runtime to change brightness
- Fade loop: 0 → 999 → 0 in 2ms steps, repeat

---

## Key config decisions

**Timer:** TIM4 on APB1. PD12–PD15 are hardwired to TIM4 CH1–CH4 via alternate function — this is the only timer that can drive these specific LED pins directly.

**Clock:** Board running at 16 MHz (not 168 MHz default). Always verify in CubeMX clock tree before calculating PSC/ARR — wrong assumption here gives wrong frequency silently.

**PSC = 15, ARR = 999:**
- 16 MHz ÷ (15+1) = 1 MHz counter clock
- 1 MHz ÷ (999+1) = 1 kHz PWM frequency
- CCR range 0–999 gives 1000 steps of brightness — smooth enough for visible fading

**No NVIC:** Basic PWM needs no interrupt. Timer runs and drives pins fully in hardware after `HAL_TIM_PWM_Start`.

---

## Gotchas

- PSC and ARR depend entirely on your actual timer clock — verify in clock tree first. My board runs at 16 MHz so values differ from most tutorials which assume 84 MHz on APB1.
- `HAL_Delay` in the fade loop is not blocking PWM — PWM is hardware. The delay just slows down how fast CCR steps change so the eye can perceive the fade.
- All four channels share the same counter (same frequency) but each has its own CCR (independent brightness).

---

## Lessons learned

**PWM runs in hardware — CCR is just a number you write.**
Once `HAL_TIM_PWM_Start` is called the timer drives the pin on its own. Changing brightness is just writing a new value to CCR via `__HAL_TIM_SET_COMPARE`. No need to stop, reconfigure, or restart anything.

**Verify clock tree before calculating timings.**
Calculated PSC=83 for 84 MHz (from tutorial) but board was at 16 MHz — correct value turned out to be PSC=15. Same formula, completely different numbers. The clock tree screenshot in CubeMX is the ground truth.

**ARR controls resolution, not just frequency.**
ARR=99 gives 1 kHz but only 100 brightness steps — visibly coarse. ARR=999 gives 1 kHz with 1000 steps — smooth fade. When choosing ARR, think about how many steps you need, not just the target frequency.
