# DHT11_DWT_UART

Bit-banged DHT11 temperature/humidity sensor on a single GPIO pin (PA1), using the
DWT cycle counter for microsecond-precision timing. Decoded values printed over
USART2.

## What it does

DHT11 has no dedicated peripheral backing it - it's a single-wire digital protocol
where the MCU and sensor take turns driving one line, and every bit is decoded by
measuring how long the line stays high. The whole project lives in two files:
`dwt_delay.c/h` (reusable microsecond delay, not DHT11-specific) and `dht11.c/h`
(start signal, single-bit read, full 40-bit frame read with checksum validation).

## Hardware

- DHT11 3-pin breakout (has onboard pull-up, no external resistor needed)
- VCC -> 3V, GND -> GND, DATA -> PA1

## Key config decisions

- **DWT cycle counter over a TIM peripheral** for `delay_us()` - frees up a timer
  for later projects, and is the standard choice for 1-Wire bit-banging since it's
  just a free-running counter with no peripheral setup beyond two register writes.
- **PA1 mode is toggled in software** between output (driving the start signal) and
  input (reading the response + data bits), using `HAL_GPIO_Init()` rather than
  direct register writes. This is fine because the mode switches themselves aren't
  on the critical timing path - only the bit-decode loop is, and that loop just
  reads `IDR` via `HAL_GPIO_ReadPin()`, no mode switching inside it.
- **Every wait loop has a timeout** (100-150us). Without it, a disconnected sensor
  or floating pin hangs the MCU forever in a `while` loop with no way out.
- **Bit decode threshold: >40us high duration = 1, else 0.** Real values are
  ~27us (0) and ~70us (1), so 40us sits in the middle with margin on both sides.
- **Checksum validated inside `DHT11_Read()`**, not left to the caller - a
  corrupted frame and a failed start signal both just return 0, so `main.c` only
  has one failure case to check.

> [!WARNING]
> `DHT11_Start()` must explicitly re-configure PA1 to output mode at the very top
> of the function, every call. The previous call leaves PA1 in input mode (it has
> to, to read the response), and `HAL_GPIO_WritePin()` is silently a no-op on a
> pin configured as input - it writes the output register but the pin never
> actually changes state. Skipping this re-init meant only the first read after
> reset ever succeeded; every read after that returned a timeout.

## Gotchas

- `DWT_Init();` was first pasted just above `main()`'s opening brace, in the
  function-prototype area, instead of inside the function body - produced an
  implicit-int compiler error since a bare function call outside any function
  looks like a malformed declaration to the compiler.
- Moving code between CubeMX's `USER CODE BEGIN/END` markers broke brace matching
  twice - once leaving `main()` with one closing brace short, once with the
  toggle-test code stuck outside the markers (which would've been silently wiped
  on a CubeMX regen).
- The toggle/delay sanity-check loop had to be fully removed before
  `DHT11_Start()` could take ownership of PA1 - leaving both in would have them
  fighting over the pin every loop iteration.

## Lessons learned

- Unsigned subtraction (`DWT->CYCCNT - start`) handles the 32-bit counter's
  wraparound correctly with no special-casing needed - comparing against
  `start + ticks` directly would break the moment the counter wraps.
- GPIO mode has to be explicitly re-asserted at the start of any bit-banged
  protocol function if an earlier call in the same function left the pin in a
  different mode. This isn't HAL-specific - it's just what "the pin is in input
  mode" means at the register level.
- `HAL_GPIO_Init()` overhead is fine for one-time mode transitions that happen
  outside a tight timing window, but would not be acceptable inside the 40-bit
  decode loop itself.

> [!NOTE]
> Validated behaviorally only this round - sane room values (50% / 24C baseline)
> and a correct, physically plausible response to a lighter held near the sensor
> (temp climbing, humidity swinging as the air heated). Did not cross-check the
> decoded bit timing against the datasheet on the logic analyzer, and did not
> confirm the decimal bytes are 0 as the