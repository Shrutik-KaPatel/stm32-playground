# UART RX + DMA — 10-byte receive on STM32F407

Receives exactly 10 bytes over USART2 using DMA1 in Normal mode. DMA fills the buffer silently without CPU involvement. When the 10th byte arrives, DMA fires a callback — the CPU processes the data once, then DMA restarts to listen again.

---

## Hardware

| Item | Detail |
|------|--------|
| Board | STM32F407G-DISC1 |
| UART TX (board output) | PA2 → USB-TTL RX |
| UART RX (board input) | PA3 → USB-TTL TX |
| Baud rate | 115200 |
| Monitor port | /dev/ttyUSB0 |

---

## How it works

```
You type 10 chars in screen terminal
    → USART2 receive register gets each byte
    → DMA1 Stream5 grabs each byte silently
    → fills rx_buffer[] in RAM
    → after 10th byte: HAL_UART_RxCpltCallback fires
    → callback prints the buffer over UART
    → callback restarts DMA to listen again
```

6 bytes = 1 interrupt. CPU only involved once at the end — not per byte.

---

## CubeMX configuration

**USART2**
- Mode: Asynchronous
- Baud Rate: 115200
- Direction: Receive and Transmit

**DMA1 Stream5**
- Direction: Peripheral to Memory
- Mode: Normal
- Data Width: Byte (both sides)
- Memory Increment: ON
- Peripheral Increment: OFF

**NVIC**
- DMA1 Stream5 global interrupt: enabled
- USART2 global interrupt: enabled

---

## Key code

```c
/* USER CODE BEGIN PV */
uint8_t rx_buffer[10];

/* USER CODE BEGIN 2 */
HAL_UART_Receive_DMA(&huart2, (uint8_t*)rx_buffer, 10);

/* USER CODE BEGIN 4 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if(huart->Instance == USART2)
    {
        printf("Received: %s\r\n", (char*)rx_buffer);
        HAL_UART_Receive_DMA(&huart2, rx_buffer, 10);  // restart DMA
    }
}

int _write(int file, char *ptr, int len)
{
    HAL_UART_Transmit(&huart2, (uint8_t*)ptr, len, HAL_MAX_DELAY);
    return len;
}
```

`HAL_UART_Receive_DMA` must be called again inside the callback — in Normal mode DMA stops after filling the buffer and never restarts on its own.

---

## Testing

```bash
screen /dev/ttyUSB0 115200
```

Type exactly 10 characters directly in screen — no Enter. Screen does not echo what you type. After the 10th character the board responds:

```
Received: helloworld
```

---

## Important notes

**ttyUSB0 vs ttyACM0** — use `ttyUSB0` (USB-TTL adapter) for UART monitoring. `ttyACM0` is the ST-Link debugger port and is not connected to USART2.

**ModemManager** — installing minicom on Ubuntu pulls in ModemManager which automatically hijacks serial ports. Disable it:

```bash
sudo systemctl disable ModemManager
sudo systemctl stop ModemManager
```

**screen sessions** — always check for zombie screen sessions if port appears busy:

```bash
screen -list
screen -X -S <pid> quit
```
