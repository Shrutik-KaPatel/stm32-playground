# MEMS_TiltLEDs

A real-time tilt indicator built on the STM32F4DISCOVERY using the onboard
LIS3DSHTR MEMS accelerometer. Tilt the board in any direction and the
corresponding LED lights up instantly. The system is fully interrupt and
DMA driven - the CPU never polls or waits.

---

## What It Does

On boot, four LEDs rotate anticlockwise as a visual prompt. The user positions
the board at their preferred reference angle and presses the onboard button to
lock it in as the baseline. From that point, any tilt away from the baseline
lights the corresponding directional LED:

- Tilt North → green LED (PD12)
- Tilt South → blue LED (PD15)
- Tilt East → red LED (PD14)
- Tilt West → orange LED (PD13)

Live XYZ acceleration and delta values stream over UART throughout, useful
for monitoring and threshold tuning.

---

## Hardware

- **Board:** STM32F4DISCOVERY (MB997-F407VGT6-E01, STM32F407VGT6)
- **Sensor:** LIS3DSHTR (U5, onboard 3-axis MEMS accelerometer)
- **SPI1:** PA5 = CLK, PA7 = MOSI, PA6 = MISO
- **CS:** PE3 (software controlled)
- **INT1/DRDY:** PE0 (EXTI0, rising edge)
- **Button:** PA0 (B1, external 10kΩ pull-down on board)
- **LEDs:** PD12 (green), PD13 (orange), PD14 (red), PD15 (blue)
- **UART:** PA2 TX, PA3 RX, USART2, 115200 baud

---

## Custom SPI Driver Library

This project includes a portable SPI driver library for the LIS3DSHTR written
from scratch - `LIS3DSHTR.c` and `LIS3DSHTR.h`. The library uses a handle/struct
pattern so it is not hardcoded to any specific SPI peripheral or CS pin:

```c
typedef struct {
    SPI_HandleTypeDef *hspi;    // which SPI peripheral
    GPIO_TypeDef      *cs_port; // CS GPIO port
    uint16_t           cs_pin;  // CS GPIO pin
} LIS3_HandleTypeDef;
```

Public API is intentionally minimal:
- `LIS3_Init` - configures ODR and axes via CTRL_REG4
- `LIS3_ReadXYZ` - reads all six output registers
- `LIS3_WriteReg` - writes any register by address

Internal helpers (`LIS3_ReadReg`) are not exposed in the header - anyone using
the library only sees what they need. The same library was developed and tested
across three earlier projects before being integrated here.

---

## Architecture

### Calibration Phase
TIM2 fires every 200ms and rotates a single LED anticlockwise around the four
user LEDs - a visual "waiting" indicator. The main loop polls B1 (PA0). When
pressed, 10 accelerometer samples are taken and averaged to establish baseline
X and Y values for the user's chosen reference position. TIM2 stops and all
LEDs turn off, signalling the system is live.

Calibration by button press was a deliberate design decision - the board sits
in a helping-hands tool at a fixed but non-horizontal angle, so startup
auto-calibration would produce a wrong reference. User-triggered calibration
solves this cleanly.

### Tilt Detection Phase
The LIS3DSHTR pulses INT1 (PE0) at 100Hz when new data is ready (DRDY signal,
configured via CTRL_REG3). The EXTI0 rising edge interrupt fires, sets a flag,
and the main loop kicks off a 7-byte DMA SPI burst starting at OUT_X_L (0x28).
The chip auto-increments register addresses (ADD_INC=1 by default in CTRL_REG6),
returning X_L, X_H, Y_L, Y_H, Z_L, Z_H in one transaction. DMA signals
completion via HAL_SPI_TxRxCpltCallback. The main loop reconstructs signed
16-bit X and Y, computes the delta from baseline, and lights the correct LED.

```
INT1 pulses HIGH (100Hz)
  -> EXTI0 ISR fires -> data_ready = 1
  -> CS LOW
  -> HAL_SPI_TransmitReceive_DMA (7 bytes, returns immediately)
  -> DMA moves data in background, CPU free
  -> TxRxCpltCallback -> CS HIGH, dma_done = 1
  -> main loop reconstructs XYZ, computes delta, updates LEDs
```

### LED Logic
Dominant axis wins - if |dx| > |dy|, X axis controls the LED, otherwise Y.
This prevents diagonal tilts from triggering two LEDs simultaneously.

```c
if (abs(dx) > abs(dy))
{
    if (dx >  TILT_THRESHOLD) // East
    if (dx < -TILT_THRESHOLD) // West
}
else
{
    if (dy >  TILT_THRESHOLD) // North
    if (dy < -TILT_THRESHOLD) // South
}
```

---

## Key Technical Decisions

**DMA SPI with multi-byte burst**
Instead of six separate blocking reads (one per output register), a single
7-byte DMA transfer reads all six output registers in one CS pulse. Reduces
CPU involvement from blocking on six transactions to zero - DMA handles
everything in the background.

**DRDY interrupt instead of polling**
The sensor signals data readiness via INT1. The MCU only wakes and initiates
a transfer when data is actually ready, at exactly the right moment. No
wasted cycles checking a status register.

**Interrupt-driven LED rotation during calibration**
TIM2 fires every 200ms and sets a flag. The main loop handles the LED state
machine. ISR stays minimal - no GPIO calls inside the timer callback.

**PA0 pull resistor**
The Discovery board has an external 10kΩ pull-down on PA0 (B1 button). The
internal pull-up is deliberately not enabled - enabling it would create a
voltage divider that holds the pin above the HIGH threshold, preventing button
presses from registering. GPIO_NOPULL is mandatory here.

**EXTI line conflict resolution**
PE0 (DRDY) and PA0 (button) both map to EXTI line 0 - STM32 shares one EXTI
line per pin number across all ports. Routing both through interrupts would
cause a conflict. PA0 is polled only during the calibration phase and never
after, making polling the clean solution.

**Threshold tuning**
Tilt threshold set to 3000 LSB after measuring actual delta values in all four
directions. Raw deltas observed: East +3691, West -6113, North +4086,
South -4396. Threshold of 3000 gives comfortable margin for deliberate tilts
while ignoring small wobbles.

---

## Peripheral Summary

| Peripheral | Role |
|------------|------|
| SPI1 | Communication with LIS3DSHTR (Mode 3, 5.25 MHz) |
| DMA2 Stream0 | SPI1 RX - sensor data to memory |
| DMA2 Stream3 | SPI1 TX - address and dummy bytes |
| TIM2 | 200ms tick for calibration LED animation |
| EXTI0 (PE0) | DRDY interrupt from sensor |
| GPIO PA0 | Button polling during calibration |
| GPIO PD12-15 | Directional LEDs |
| USART2 | Live XYZ and delta monitoring |

---

## Register Configuration

| Register | Value | Purpose |
|----------|-------|---------|
| CTRL_REG4 (0x20) | 0x67 | ODR=100Hz, all axes enabled |
| CTRL_REG3 (0x23) | 0xE8 | DRDY routed to INT1, active HIGH, pulsed |
| CTRL_REG6 (0x25) | default | ADD_INC=1 enables auto-increment for burst read |

---

## Lessons Learned

- DMA SPI with multi-byte burst is significantly more efficient than repeated
  single-register reads - one CS pulse and one callback instead of six
  blocking transactions
- User-triggered calibration is more robust than startup calibration when the
  board orientation cannot be guaranteed at boot
- EXTI line conflicts require careful planning - pin number determines line,
  not port, so PA0 and PE0 share the same line
- Dominant axis logic prevents ambiguous diagonal tilt readings cleanly
- Threshold values must be measured from real hardware - calculated guesses
  are unreliable due to sensor offset and board orientation
