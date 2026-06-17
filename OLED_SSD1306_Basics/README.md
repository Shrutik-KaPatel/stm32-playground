# OLED_SSD1306_Basics

Bring-up of a generic 0.96" SSD1306 I2C OLED module (yellow/blue two-tone, 4-pin: VCC/GND/SCL/SDA) on the STM32F4DISCOVERY. First time working with this chip family. Bare-metal HAL, no external display library - own driver written from scratch, same pattern as I2C_CS43L22_Lib.

## What it does

- Confirms the OLED on the I2C1 bus (address 0x3C, found at 0x3C in scan output - HAL takes the 8-bit shifted form internally, the printed/scanned value is the bare 7-bit address)
- Sends the SSD1306 init sequence (charge pump, mux ratio, COM config, contrast, display on)
- Implements a 1024-byte RAM framebuffer (128x64, 1 bit per pixel) with a pixel-set function, pushed to the chip via page/column addressing
- Draws primitives on top of the framebuffer: circles (Bresenham/midpoint, both outline and filled), rectangles, a hand-rolled 5x7 bitmap font for text
- Final demo: an animated two-frame "robot face" (open eyes / wink) cycling in the main loop, parked on top of all of the above

## Key config decisions

- I2C1 at Standard Mode 100 kHz, same as the CS43L22 work - no reason to introduce a second new variable while bringing up a brand-new chip
- PB9/PB6 for SDA/SCL (this board's known deviation from CubeMX's PB7 default), reused without re-deriving it
- USART2 kept active for debug prints throughout - this chip has no register map to read back, so the bus-scan ack and status checks were the only way to confirm communication was happening
- 4-pin module = no exposed RES# pin, so reset happens internally on power-up; no manual reset pulse needed in init code

## SSD1306-specific concepts (new chip family, worth recording)

- No registers. Every I2C write is prefixed by one control byte: `0x00` means "what follows is a command," `0x40` means "what follows is pixel data going straight into GDDRAM." That's the entire protocol.
- GDDRAM is organized in 8 pages of 8 vertical pixels each. One byte written = one column, one page, 8 pixels stacked vertically, bit 0 = top of that strip, bit 7 = bottom. All page/column addressing math in `OLED_UpdateScreen()`/`OLED_Clear()` comes directly from this.
- The chip needs its own charge pump enabled (`0x8D`,`0x14`) before `0xAF` (display on), because this module has no separate high-voltage VCC supply broken out - it's a 4-pin board, so the panel drive voltage is generated internally from logic-level VDD. Skipping this is the single most common "did everything right, screen stays dark" failure mode for these modules - confirmed by hitting it directly mid-session.
- Two of the init bytes (segment re-map, COM scan direction) are not chip properties - they depend on how the specific panel is wired to the SSD1306's internal SEG/COM drivers, which varies between cheap module vendors even though the driver chip is identical. No way to know the right value except by testing on hardware.

## Gotchas

- This panel is physically two-tone (yellow top strip, blue/white for the rest) - baked into the OLED material itself, not a driver or code artifact. A forced "entire display on" command lighting up both colors confirmed this rather than indicating a fault.
- A diagonal line drawn from `(0,0)` to `(63,63)` plus its mirror does **not** produce an X - it produces a V, since both lines only share an endpoint near the bottom rather than crossing through the screen's vertical center. Needed `x` swept across the full 128-wide canvas with `y` scaled by half to get diagonals that actually cross at the middle.
- A loose/marginal SDA or SCL connection on the breadboard can produce a very specific and confusing symptom: the I2C bus scan fails (`No I2C devices found`), while the OLED panel still shows whatever was last successfully written to its GDDRAM, fully lit and intact, because the chip keeps refreshing the panel from RAM independently of whether new I2C traffic is currently arriving. A still-lit, still-correct screen during a failed scan means power and prior writes were fine - it does not mean the current bus state is fine.
- Chasing a "blank screen" turned out to be two unrelated near-misses, not one bug: (1) a test using `OLED_Buffer_Clear()` + plain `0xA5` (force entire display on) had `0xA5` *not* reverted to `0xA4` afterward, so a later framebuffer test never had a chance to show anything - the chip was still ignoring RAM. (2) once that was fixed, the actual test content was five single isolated pixels, which are genuinely close to invisible on this glass at a glance. Switched to thick 8px-tall bars at the screen edges as a sanity-check primitive instead of single pixels - much harder to miss, and the right tool for "is the pipeline alive at all" versus "is the content positioned correctly."
- `HAL_I2C_Mem_Write` return values were not being checked anywhere in the driver. Worth treating as a known gap, not yet fixed - every command/data write currently assumes success.
- A misplaced closing brace (one extra `}` after the main `while(1)` loop) silently closed `main()` early, leaving `SystemClock_Config`/`MX_I2C1_Init`/`MX_USART2_UART_Init` sitting outside the function body. Caught by inspection, not by a compiler error surfacing clearly - worth a careful brace-count pass any time a large block gets pasted into `main()`.

## Lessons learned

- For a chip with no readable register map, "did communication succeed" has to be inferred indirectly - a bus-scan ack only proves one short transaction worked, not that a long sequence (full init, or an entire 1024-byte screen push) survives without a glitch. A `HAL_I2C_IsDeviceReady()` check immediately after a multi-byte sequence is a cheap way to get a second, later data point instead of trusting one early one for the whole session.
- Orientation/remap settings (segment re-map, COM scan direction) are best diagnosed with the simplest possible unambiguous shape - a plain outline rectangle is far better for this than a multi-element face, since any mismatch in axis direction or mirroring shows up immediately as a non-rectangle, where the same mismatch buried in a complex face just reads as vaguely "wrong" without telling you which transform is needed.
- When debugging a no-display symptom, "is it dark" and "is it showing the right thing" are different questions that need different tests - a forced-everything-on command (`0xA5`) isolates the analog/power path from the digital RAM-addressing path, and conflating the two cost real time in this session.

## Hardware verification

- I2C bus scan via USART2/minicom at every stage
- Logic analyzer not used for this project - debugging stayed entirely in the I2C/visual domain (bus scan + on-screen results were sufficient to localize every issue that came up)
- Visual confirmation on actual hardware for every drawing primitive (column test, X-pattern, text, circles, rectangle, full face) before moving to the next one