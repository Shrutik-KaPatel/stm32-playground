# UART_TEST

A UART serial communication project for the STM32F4DISCOVERY board.

## What it does
- Monitors the onboard user button (PA0)
- Sends a message over UART when the button state changes
- Controls the blue LED (PD15) based on button state
- Uses a timer interrupt (TIM3) for periodic button polling

## Hardware
- **Board:** STM32F4DISCOVERY (STM32F407VGT6)
- **USB-to-TTL Adapter:** Waveshare FT232RNL (set to 3.3V)

## Wiring
| Adapter | Discovery Board (P1) |
|--------|----------------------|
| TXD    | PA3 (USART2_RX)      |
| RXD    | PA2 (USART2_TX)      |
| GND    | GND                  |

## Serial Settings
- Baud Rate: 115200
- Data: 8 bits, No parity, 1 stop bit (8N1)

## Monitoring on Linux
```bash
sudo screen /dev/ttyUSB0 115200
```

## Output
```
Button Pressed: LED on
Button Released: LED off
```
