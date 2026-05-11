# UART_RX_DMA_Circular

USART2 receives 10 bytes via DMA in Circular mode. On every complete receive the callback sets a flag, and the main loop handles printing. DMA restarts automatically - no manual restart needed anywhere.

## What it does

- DMA continuously fills `rx_buffer[10]` in the background
- `HAL_UART_RxCpltCallback` sets `rx_flag = 1` only - nothing else
- Main loop detects the flag, clears it, prints the received buffer
- Repeats forever without any manual DMA restart

## Key config decisions

**Circular mode:** DMA reloads and restarts automatically after every complete transfer. `HAL_UART_Receive_DMA` called once at startup - never called again anywhere else.

**Flag pattern:** Callback only sets a flag. All processing happens in the main loop. This keeps the callback short and fast - critical in real firmware where heavy work inside a callback can cause missed interrupts or timing issues.

**NVIC enabled:** USART2 global interrupt must be enabled for the callback to fire.

## Normal mode vs Circular mode

| | Normal | Circular |
|---|---|---|
| DMA stops after N bytes | Yes | No |
| Manual restart needed | Yes, in callback | No |
| HAL_UART_Receive_DMA calls | Every callback | Once at startup |
| Use case | Fixed-length commands | Continuous data stream |

## Gotchas

- Minicom buffers keystrokes and sends all at once on Enter by default. To send character by character, enable local echo via `sudo minicom -s` -> Keyboard and Misc. Without sudo minicom cannot save settings to `/etc/minicom/minirc.dfl`.
- Leftover screen or minicom sessions hold the port - use `lsof /dev/ttyUSB0` to find and `kill <PID>` to free it.
- "Circular DMA started" appearing after typing is normal - it was buffered in the USB serial chip while minicom wasn't connected yet.

## Lessons Learned

**Circular mode means set it and forget it.**
One `HAL_UART_Receive_DMA` call at startup is all you need. The hardware handles refilling the buffer automatically. Normal mode requires manual restart in the callback - Circular mode removes that responsibility entirely.

**Flag pattern is the right way to bridge ISR and main loop.**
Doing heavy work inside a callback is risky - callbacks run in interrupt context where timing is critical. Setting a flag in the callback and processing in the main loop is cleaner, safer, and easier to debug.

**Both previous UART projects used Circular mode without knowing it.**
The real lesson here wasn't the DMA mode - it was learning the flag pattern as a cleaner design approach compared to doing everything inside the callback.
