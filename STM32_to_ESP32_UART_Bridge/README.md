# STM32_to_ESP32_UART_Bridge

## What this is

Bidirectional UART link between the STM32F407 (master) and the ESP32-WROOM-32D (slave/modem). STM32 sends `PING\n`, ESP32 receives it and replies `PONG\n`. This is the physical-link bring-up for an eventual anomaly-forwarding path (STM32 -> ESP32 -> cloud LLM) for a larger predictive-maintenance project. No protocol design beyond raw ASCII strings at this stage; framing, structured messages, and non-blocking integration with the FFT loop are deferred.

Two subfolders:
- `stm32_master/` - CubeIDE project, USART3 master
- `esp32_slave/` - ESP-IDF project, UART2 slave

## Hardware

Wiring:
- STM32 PB10 (USART3 TX) -> ESP32 GPIO16 (RX2)
- STM32 PB11 (USART3 RX) <- ESP32 GPIO17 (TX2)
- Common GND between boards (separate wire from TX/RX, both boards USB-powered independently)

115200 baud, 8N1, both sides. Both 3.3V logic, no level shifting needed.

## Key decisions

USART2 (PA2/PA3) stays the existing debug console on the STM32, so this project uses USART3 on PB10/PB11 instead, kept fully separate.

ESP32 side uses UART2 (GPIO16/17), not UART0 and not UART1's default pins. UART0 (GPIO1/3) is hardwired to the onboard CP2102 USB-serial bridge, used for flashing and `idf.py monitor` - putting the bridge there would collide with every flash/monitor session. UART1's default pins land on GPIO9/10, which on WROOM modules are tied to the integrated SPI flash and must not be touched. GPIO16/17 are free on the WROOM-32D (no PSRAM on this variant, unlike WROVER where those pins are reserved).

LED indicators (visual only, no logic-analyzer dependency once link is wired):
- STM32 PD15 (blue) - toggles every transmit cycle, heartbeat for "main loop is alive"
- STM32 PD12 (green) / PD13 (orange) - intended as reply-confirmed / timeout indicators
- ESP32 GPIO2 (onboard LED) - lights briefly on any UART2 receive

## Gotchas

Initial `main.c` redirected `printf` to `huart2` via `_write()`, copied in deliberately on the assumption debug printf would be useful here too, same as other projects. This project never configured USART2 though, so it failed to compile. Removed the printf/`_write()` path entirely since debug output isn't needed here, the LEDs and logic analyzer cover it.

Fixed-length blocking receive was an early bug: `HAL_UART_Receive(&huart3, rxBuf, sizeof(rxBuf) - 1, 500)` waits for the *full* buffer length before returning OK, not "whatever showed up." Requesting 31 bytes when the reply is only 5 (`PONG\n`) meant it always timed out regardless of whether the reply had arrived. Fixing the requested length to match didn't fully resolve it either, see below.

Early wiring mistake: wires went to PE10/PE11 instead of PB10/PB11, the pins USART3 was actually configured for. With nothing connected to the real TX/RX pins, both lines floated and picked up noise, read as continuous garbage (ESP32 logging `Got 255 bytes` nonstop, no real PING content). Moving the wires to the correct PB10/PB11 pins fixed actual data flow, confirmed when `Got 5 bytes: PING` started showing cleanly.

ESP32 buffer backlog: if the STM32 has been transmitting for a while before the ESP32 finishes booting (or before a UART driver reinstall), bytes queue up in the driver's ring buffer. The first `uart_read_bytes` call then drains everything queued at once, showing as a single oversized read (`Got 255 bytes: PING` containing many concatenated `PING\n` repeats). Added `uart_flush(UART_NUM)` right after driver install as a general hygiene fix; the actual long-term fix is to not let one side free-run indefinitely with no listener, which is a protocol-design problem to solve later, not here.

Dropped `HAL_Delay(1000)`: during an LED-logic edit (adding the orange timeout / red error indicator), the trailing `HAL_Delay(1000)` got lost from the loop. Without it the STM32 transmits as fast as the transmit/receive calls allow, well above 1/sec, which re-triggered the same ESP32-side batching symptom (`Got 10 bytes`, `Got 15 bytes`). Restoring the delay fixed the transmit cadence.

Duplicate ESP32 logic: a nested, duplicate `if (strncmp(...PING...))` block caused PONG to be sent twice per received PING. Removed the inner duplicate, kept one clean send-and-log per PING.

Persistent STM32 timeout despite confirmed-clean wire: this was the hardest one. Wiring, ground, baud, and byte content were all verified correct using PulseView (fx2lafw driver, 1MHz sample rate) with the UART protocol decoder loaded on both PB10 and PB11 - decoded bytes showed clean `PING` outbound and clean `PONG` inbound, correct baud, no framing errors. Despite this, `HAL_UART_Receive` kept timing out (never `HAL_OK`, never an overrun flag either). Capturing both lines on the same timeline showed the actual cause: the STM32's blocking transmit-then-receive loop runs out of phase with the ESP32's reply timing - the PONG for one cycle often lands during or after the window for the next cycle has already started, so a same-cycle blocking receive call misses it. An interrupt-driven receive (`HAL_UART_Receive_IT`, always armed via a callback) is the structurally correct fix, but the first implementation attempt introduced its own regression (burst transmits) and was reverted. The link itself was not in question by this point, only the STM32-side visual confirmation LED.

## Outcome

Link confirmed bidirectionally via logic analyzer decode: STM32 transmits clean `PING\n`, ESP32 receives and replies clean `PONG\n`, both at 115200 8N1 with no framing errors. That's the actual bring-up goal here.

The STM32-side green "reply confirmed" LED never reliably lit due to the phase-alignment issue above. Not fixed in this pass - the correct fix is a proper interrupt-driven RX architecture (always-armed receive with a flag set in the callback, main loop never blocks on receive), which is also the right pattern for the capstone where RX needs to coexist with the FFT loop. Deferred as a future pass rather than patched further here, since the physical link itself is already proven.

## Lessons learned

A logic analyzer with a protocol decoder loaded (not just raw waveform) is decisive for separating "is the data actually correct" from "is the software logic correct." Several turns were spent on wire/baud/overrun theories that a raw waveform couldn't rule out by itself; decoding actual byte values on both lines in one capture settled it immediately.

`HAL_UART_Receive` with a fixed expected length only returns `HAL_OK` once exactly that many bytes arrive - mismatched expected-length versus actual reply length is an easy way to manufacture a timeout that looks like a hardware problem but isn't.

Blocking transmit-then-receive-with-timeout is fragile for a fast request/response link where the reply can arrive at any point relative to the loop's own processing delays. Interrupt-driven, always-armed RX is the correct pattern, not a one-off fix for this project.

ESP32 UART driver buffers can silently accumulate backlog if the peer starts transmitting before the listener is ready or during a reinstall. Flushing at init is cheap insurance regardless of whether it's the actual root cause of a given symptom.

On generic ESP32-WROOM-32D breakouts: UART0 (GPIO1/3) is reserved for the onboard USB-serial bridge, and GPIO6-11 are reserved for the internal SPI flash. UART2's default pins (GPIO16/17) are the safe default for a second UART on this module.
