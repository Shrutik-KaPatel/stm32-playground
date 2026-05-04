# PWM_BASIC — TIM2 PWM on PA0 → Blue LED

## What this does

Generates a PWM signal using TIM2 Channel 1 on pin PA0, then physically wires it to PD15
(blue LED) to see dimming in action. Also uses `printf` over UART2 to print a tick counter
every second just to confirm the timer and UART plumbing are both alive at the same time.

---

## Hardware setup

A plain jumper wire from **PA0 → PD15**. That's it.

The reason this works: PA0 is the physical output pin for TIM2_CH1. PD15 is where the
blue LED lives (LD6), but it isn't configured as an output in this project — it's just floating
in default input mode, so it happily follows whatever voltage PA0 drives into it.

One thing to keep in mind: PA0 is also physically tied to the USER button (B1) on the
board. Doesn't cause issues since the button is normally open, but don't press it while
testing — it'll pull the line to ground and kill the signal.

---

## Timer config

The STM32F407 runs at 168 MHz. TIM2 sits on APB1, which runs at 42 MHz, but the timer
input clock gets doubled to **84 MHz** by the APB prescaler.

From there:

```
Prescaler = 7999  →  84,000,000 / 8000 = 10,500 Hz (timer tick rate)
Period    = 999   →  10,500 / 1000     = 10.5 Hz   (PWM frequency)
Pulse     = 700   →  700 / 1000        = 70%        (duty cycle)
```

So the LED is ON 70% of the time and OFF 30%, switching at 10.5 Hz. At that frequency
you can clearly see the flicker — the eye picks up switching below roughly 60 Hz. To get
smooth visible dimming, frequency needs to be at least a few hundred Hz. That means
either shrinking the Period (fewer steps per cycle) or cranking down the prescaler. For now
this is fine since the goal was just to prove PWM is working.

The timer is initialized with `HAL_TIM_PWM_Init` and the channel configured with
`HAL_TIM_PWM_ConfigChannel`. Then started in `main` with `HAL_TIM_PWM_Start`. Once
started, the hardware drives PA0 automatically — the CPU is completely uninvolved.

---

## UART / printf

`printf` is redirected to UART2 by implementing `_write`. When `fd` is 1 (stdout) or 2
(stderr), it calls `HAL_UART_Transmit` on `huart2`. UART2 TX is on PA2, which on the
Discovery board connects to the ST-LINK's virtual COM port — so output shows up in
minicom/screen/PuTTY at 115200 baud without any extra hardware.

The tick loop in `main` uses `uwTick` (HAL's millisecond counter, incremented by SysTick
interrupt) to print once per second. Nothing to do with TIM2 — just a sanity check that the
whole system is running.

---

## Gotchas

**Double output bug** — the original version had `TICK_DELAY 500` and was printing
`now / 1000` as the tick label. Since the tick fired every 500ms but the label was in whole
seconds, two consecutive ticks always printed the same second number. Looked like
duplicate output. Fixed by changing `TICK_DELAY` to 1000.

**Stray `)` in format string** — `printf("Tick %lu (loop = %lu)\r\n)")` had a rogue closing
paren outside the string. Compiled fine since it's inside the string, just printed a literal `)`.
Easy to miss.

**`HAL_TIM_Base_Start_IT` is still there** — the interrupt-based timer start is called but
there's no `HAL_TIM_PeriodElapsedCallback` implemented, so no interrupt actually does
anything. It's harmless but leftover from the previous version. Could remove it.

---

## Lessons learned

- PWM frequency and duty cycle are completely independent. Frequency comes from
  Period + Prescaler. Duty cycle comes from Pulse / Period. Changing Pulse doesn't
  affect frequency at all.

- The timer runs entirely in hardware once started. `HAL_TIM_PWM_Start` is the last
  thing you call and then you can forget about it — no polling, no interrupts needed for
  basic PWM.

- APB1 timer clock is double the APB1 bus clock when the APB prescaler is > 1. This
  caught me off guard — the system clock is 168 MHz, APB1 bus is 42 MHz, but TIM2
  input is 84 MHz. Always check the clock tree in CubeMX before calculating prescalers.

- PD15 doesn't need to be configured as an output to be driven by an external signal.
  Floating input pins will follow whatever you connect to them. Convenient for quick
  testing, but not something to rely on in a real design.
