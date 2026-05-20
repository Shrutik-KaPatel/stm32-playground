# IWDG_Watchdog

Implements the Independent Watchdog on the STM32F407 to automatically reset the MCU when firmware stops responding.

## What it does

- Configures IWDG with a ~4 second timeout (prescaler 32, reload 4095)
- Kicks the watchdog every 500ms in the main loop via `HAL_IWDG_Refresh()`
- Simulates a firmware hang with a 5 second `HAL_Delay()` — longer than the timeout — to deliberately trigger a reset
- On each boot, reads RCC_CSR flags to identify whether the reset was caused by the watchdog or a normal power-on
- Prints reset cause and loop status over USART2 at 115200 baud

## Key config decisions

- Prescaler 32, reload 4095 - timeout = (4095+1) x 32 / 32000 = 4.096s — long enough to test comfortably without being impractically slow
- IWDG clocked from LSI (~32kHz) — independent of the system clock, keeps running even if the main clock fails
- Reset flags cleared after reading — if skipped, the watchdog flag persists into the next boot and gives a false reading

## Gotchas

- CubeMX does not display the calculated timeout value for IWDG — had to verify manually using the formula: `(RLR + 1) x Prescaler / LSI_freq`
- IWDG is found under System Core in CubeMX, not under Timers — easy to miss

## Lessons learned

- Watchdog kick belongs inside the while(1) loop, not once at startup — the whole point is proving the loop is still executing each iteration; a one-time kick at startup proves nothing
- Once IWDG is started it cannot be stopped in software — from that point on kicks are mandatory, no exceptions
- `__HAL_RCC_CLEAR_RESET_FLAGS()` must be called after reading reset cause — otherwise stale flags from a watchdog reset survive into the next power-on and give a false reading
- Watchdog timeout must be longer than the worst-case loop execution time — too short and healthy firmware trips it; too long and a real hang takes forever to recover
