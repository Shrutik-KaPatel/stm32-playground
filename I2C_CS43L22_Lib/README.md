# I2C_CS43L22_Lib

A portable I2C driver library for the CS43L22 audio DAC on the STM32F4DISCOVERY board. Built as a learning exercise in library architecture - separating HAL calls behind an abstraction layer so the core logic stays platform-agnostic.

---

## What It Does

Initializes the CS43L22 audio DAC over I2C using the required startup sequence from the Cirrus Logic datasheet. Configures headphone output routing and volume. Verifies communication via I2C bus scan and prints debug output over UART. I2S audio data transmission is the next step.

---

## Hardware

- Board: STM32F4DISCOVERY (MB997-F407VGT6-E01)
- Chip: CS43L22 audio DAC (U7)
- I2C pins: PB6 = SCL, PB9 = SDA
- Reset pin: PD4 (active low, must be driven HIGH before I2C)
- UART debug: PA2 = TX, PA3 = RX via USART2 at 115200 baud
- I2S pins (next session): PC7 = MCLK, PC10 = SCLK, PC12 = SDIN, PA4 = LRCK

---

## Public API

```c
HAL_StatusTypeDef CS43L22_Init(CS43L22_HandleTypeDef *hcs43);
HAL_StatusTypeDef CS43L22_SetOutputMode(CS43L22_HandleTypeDef *hcs43);
HAL_StatusTypeDef CS43L22_SetVolume(CS43L22_HandleTypeDef *hcs43, uint8_t volume);
```

---

## Key Config Decisions

**I2C address is 0x4A (7-bit), not 0x94**
The schematic shows 0x94 which is the full write byte - 7-bit address shifted left with R/W = 0. HAL expects the 7-bit address, so 0x4A is what gets passed. AD0 pin is tied to GND on this board, confirmed from schematic.

**PD4 must go HIGH before any I2C communication**
The CS43L22 holds all registers in reset and ignores I2C while RESET is LOW. Driving PD4 HIGH releases the chip. A 10ms delay after is needed to let it wake up properly. Skipping this means the entire bus scan returns nothing.

**Required init sequence from datasheet section 4.11**
Cirrus Logic mandates 5 specific register writes after power-up before the chip behaves correctly. These aren't optional tweaks - skipping them causes undefined behaviour. The sequence includes a pulse on a reserved register (0x32) which is a one-shot internal calibration trigger.

**Power-up register 0x02 = 0x9E**
Only three valid states exist for this register - all others are reserved. 0x9E (1001 1110) = Powered Up. 0x01 (default) and 0x9F both = Powered Down.

**Output routing register 0x04 = 0xAF**
```
PDN_HPB = 10 (headphone B always on)
PDN_HPA = 10 (headphone A always on)
PDN_SPKB = 11 (speaker B always off - not connected on this board)
PDN_SPKA = 11 (speaker A always off - not connected on this board)
```

**Volume register 0x22/0x23 = 0x00 for maximum**
Scale is inverted - 0x00 = 0dB (maximum, no attenuation). Value increases in -0.5dB steps. 0x01 = muted. Both channels A and B written with the same value.

**PB9 not PB7 for SDA**
CubeMX defaulted I2C1_SDA to PB7. The schematic clearly shows PB9. Always verify pin assignments against the schematic.

---

## Project Structure

```
Drivers/
  CS43L22/
    Inc/cs43l22.h    - public API, handler struct, register map, debug toggle
    Src/cs43l22.c    - WriteReg abstraction layer, Init, SetOutputMode, SetVolume
Core/
  Src/main.c         - handler setup, bus scan, calls public API
```

The handler pattern (CS43L22_HandleTypeDef) owns the I2C handle pointer and device address. No global variables. Supports multiple chip instances on the same bus by passing different handlers.

Internal functions are marked `static` - HAL calls are hidden inside `CS43L22_WriteReg` and never exposed publicly. Porting to another platform means changing only that one function.

---

## Gotchas

- CubeMX defaulted SDA to PB7 instead of PB9 - caused persistent NACKs until caught by comparing schematic to .ioc
- I2C scan must run AFTER PD4 is driven HIGH - confirmed with logic analyzer showing I2C activity before PD4 transition
- Volume scale is inverted - 0x00 is maximum volume, not 0xFF
- Speaker output exists in the chip but is not connected on the STM32F4DISCOVERY board
- `return HAL_OK` is required - function declares HAL_StatusTypeDef return type
- `#include "main.h"` needed in cs43l22.c for CubeMX-generated GPIO macros
- Two functions with the same name won't compile - both volume channels go inside one function

---

## Lessons Learned

- Always verify pin assignments against the schematic, not just CubeMX defaults.
- The logic analyzer caught what serial output couldn't - "Init complete" printed while every I2C transaction was NACKing. Hardware tells the truth.
- Hardware enable pins and software power registers serve different purposes. RESET controls whether the chip responds at all. Power register controls which internal blocks are active.
- I2C addresses in datasheets are often written as the full 8-bit write byte. HAL expects the 7-bit address. Always check which format is being used.
- Reserved register writes in datasheets are not mistakes. Follow the datasheet sequence exactly.
- The handler/pointer pattern lets one function serve multiple chip instances.
- Headphone A/B and Speaker A/B are left/right stereo channels, not separate outputs.
- Headphone and speaker amps serve different loads - headphone drives high impedance, speaker has a class D amp for low impedance loads.
