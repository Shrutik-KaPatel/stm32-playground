# WWDG_Watchdog

Implements the Window Watchdog on the STM32F407 to reset the MCU when firmware runs too fast or too slow, catching a different class of bug than IWDG.

## What it does

- Configures WWDG with a valid kick window between ~36.6ms and ~49.1ms
- Kicks the watchdog every 40ms via `HAL_WWDG_Refresh()`, comfortably inside the window
- Demonstrates both failure modes: kicking too late (60ms delay) and too early (5ms delay), both trigger a reset

## Key config decisions

- Prescaler 8, counter 127, window 80 - chosen to give a testable window in the tens of milliseconds range
- Valid kick window: counter must be below 80 but above 64 - calculated as (127-80) x 0.78ms = 36.6ms minimum wait, (80-64) x 0.78ms = 12.5ms window width
- Tick duration = 1 / (42MHz / 4096 / 8) = ~0.78ms - WWDG is clocked from APB1 at 42MHz, much faster than IWDG's LSI

## Gotchas

- Window value of 64 (default) means zero window width - you can never kick safely. Always set window above 64.
- WWDG timeouts are in milliseconds not seconds - easy to miscalculate coming from IWDG
- Kicking too early resets you just as hard as kicking too late - both failure modes were verified on hardware

## Lessons learned

- WWDG catches a different bug class than IWDG - code running faster than expected (e.g. a sensor read returning instantly because the sensor is disconnected) would trip WWDG but sail through IWDG undetected
- The valid window sits between the window value and 0x40 (64) - the counter must drop below the window value before a kick is accepted
- WWDG is clocked from APB1 and stops in debug mode by default - timeouts behave differently when stepping through code in the debugger versus running freely
