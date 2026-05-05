# ADC1 + DMA — Continuous Sampling on STM32F407

Reads analog voltage on PA0 (user button) using ADC1 with DMA in circular mode. The CPU never handles the data transfer — DMA writes the result directly into RAM continuously. Value is printed over UART2 at 115200 baud every 500ms.

---

## Hardware

| Item | Detail |
|------|--------|
| Board | STM32F407G-DISC1 |
| Analog input | PA0 (user button / ADC1_IN0) |
| UART TX | PA2 (USART2) |
| Baud rate | 115200 |

---

## How it works

ADC1 runs in continuous mode. On every conversion it sends a DMA request. DMA2 Stream0 Channel0 picks it up and writes the 16-bit result into `adc_value` in RAM. The CPU never wakes up — it just reads `adc_value` whenever needed.

```
ADC1 finishes conversion
    → sends DMA request (hardware signal)
    → DMA2 Stream0 reads ADC1->DR
    → DMA2 writes into adc_value in RAM
    → CPU reads adc_value in while loop
```

---

## CubeMX configuration

**ADC1**
- Channel: IN0 (PA0)
- Continuous Conversion Mode: Enable
- DMA Continuous Requests: Enable
- Sampling Time: 480 Cycles

**DMA2 Stream0**
- Direction: Peripheral to Memory
- Mode: Circular
- Data Width: Half Word (both sides)
- Memory Increment: Disabled

**NVIC**
- DMA2 Stream0 global interrupt: enabled, Priority 5

---

## Key code

```c
/* USER CODE BEGIN PV */
volatile uint32_t adc_value = 0;

/* USER CODE BEGIN 2 */
HAL_ADC_Start_DMA(&hadc1, (uint32_t*)&adc_value, 1);

/* USER CODE BEGIN 3 */
printf("ADC: %lu\r\n", adc_value);
HAL_Delay(500);
```

`volatile` is required — DMA writes from hardware and the compiler has no visibility into that. Without it the compiler caches the value and it never updates.

`uint32_t` is used despite ADC being 16-bit — it guarantees 4-byte memory alignment which DMA requires. The 16-bit result lands in the lower half.

---

## Expected output

```
ADC: 2       ← PA0 floating / at GND
ADC: 4095    ← PA0 at 3.3V (button pressed)
```

---

## Troubleshooting encountered

**DMA mode generated as DMA_NORMAL**
CubeMX generated `DMA_NORMAL` in `stm32f4xx_hal_msp.c` despite Circular being set in the GUI. DMA transferred once then stopped. Always verify `DMA_CIRCULAR` in the MSP file after regeneration.

**HardFault inside HAL_ADC_Start_DMA**
Type mismatch between variable size and DMA width caused a memory alignment fault. Board crashed and reset before any further code ran — visible as "Board started" repeating endlessly. Fixed by using `uint32_t` variable with `HALFWORD` DMA width.

**DMA interrupts flooding CPU**
At 3 Cycles sampling time ADC fired ~500,000 conversions per second. DMA interrupts at priority 0 starved SysTick completely — `HAL_Delay` locked up the board. Fixed by increasing sampling time to 480 Cycles and DMA interrupt priority to 5.
