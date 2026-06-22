# KY024_EXTI_RPM

Hall-effect tachometer using the KY-024 linear hall sensor's digital threshold
output, EXTI interrupt for pulse counting, and a once-per-second RPM calculation.
Built to cross-reference fan rotational speed against the capstone's
vibration-based fault classification.

## What it does

A small magnet glued to one fan blade passes the KY-024 once per revolution.
Each pass triggers a falling-edge EXTI interrupt on PC0, which increments a
counter. The main loop reads and resets that counter once per second and
converts it directly to RPM (count x 60).

## Hardware

- KY-024 hall sensor, 4-pin breakout (+, G, A0, D0)
- VCC -> 3V, GND -> GND, D0 -> PC0
- A0 left unconnected for the final RPM logic, but was wired to PA0/ADC1_IN0
  temporarily as a diagnostic tool (see Gotchas)
- Small magnet glued flat to one blade face (not the edge, to avoid adding
  drag or throwing off blade balance)

## Key config decisions

- **Digital output + EXTI, not analog + ADC polling.** The goal is counting
  discrete events (one pulse per revolution), not measuring field strength -
  an interrupt-driven threshold crossing is the right tool for that, and
  doesn't require fighting ADC sample timing for what's fundamentally a
  yes/no event.
- **`volatile` on the pulse counter** - it's written inside an ISR and read
  in `main()`. Without `volatile` the compiler could cache a stale value in
  a register under optimization and silently break the count.
- **Read-then-reset combined in one function** (`Tach_ComputeRPM()`), not
  left as two separate steps for the caller - minimizes (though doesn't
  fully eliminate) the race window where a pulse could land between a
  separate read and reset.
- **`HAL_Delay(1000)` for the 1-second counting window.** Acceptable for this
  standalone bring-up; flagged as something that needs to change before this
  goes into the capstone, where the main loop can't tolerate a full second
  blocked while FFT/UART work also needs to run.

## Gotchas

- KY-024's onboard comparator threshold (set via an onboard trimpot) needed
  manual tuning - out of the box, the magnet had to be held nearly against
  the sensor before D0 would trigger at all.
- Added a temporary ADC1/PA0 diagnostic read of the A0 pin to see raw field
  strength while turning the trimpot, since the trimpot adjusts the
  comparator's reference voltage, not the A0 signal itself - A0 reads the
  same raw field strength regardless of trimpot position.

> [!WARNING]
> Once mounted on the actual fan motor and tested across all 3 speed levels,
> the pulse counts showed clear electrical noise contamination - long clean
> stretches of 0 RPM interrupted by physically impossible spikes (one reading
> hit 18,660 RPM, equivalent to 311 revolutions per second). This is most
> likely EMI from brush arcing in the fan's DC motor, picked up because the
> comparator threshold is currently set too close to maximum sensitivity.
> Some clusters of consistent, plausible readings (1020-1200 RPM range) did
> appear, suggesting the real signal is present but currently buried in noise
> rather than absent.

## Lessons learned

- A working sensor in a quiet hand-test (waving the magnet by hand on a desk)
  does not guarantee a clean signal once mounted near a running motor - real
  electrical noise sources only show up under real operating conditions.
- "More sensitive" and "correct" are different tuning goals for a comparator
  threshold - maximum sensitivity just means triggering on the weakest
  possible signal, which includes noise. The right threshold rejects noise
  while still catching the real event, which is a narrower target than just
  cranking sensitivity to maximum.
- A software plausibility filter (rejecting pulse intervals faster than any
  physically possible RPM) is a reasonable complement to hardware tuning, but
  shouldn't substitute for fixing the actual noise source at the hardware
  level first.

> [!NOTE]
> Functional but not yet clean. Pulse-counting mechanism, EXTI wiring, and
> RPM math are all validated as correct in principle (the plausible-range
> clusters prove this). What remains unresolved - trimpot re-tuning to reject
> motor EMI, and/or a software debounce/plausibility filter - is deferred to
> the capstone integration phase rather than solved here, per the original
> plan to de-risk the mechanism in isolation before full integration.