# SPI_LIS3DSHTR_Accel

## What It Does

Reads live X, Y, Z acceleration data from the LIS3DSHTR MEMS accelerometer over SPI
and prints raw 16-bit values over UART at 100 Hz. Confirms the full read/write register
workflow works before building a library on top of it.

## Hardware

- Board: STM32F4DISCOVERY (MB997-F407VGT6-E01)
- Sensor: LIS3DSHTR (U5, onboard MEMS accelerometer)
- SPI1: PA5 = CLK, PA7 = MOSI, PA6 = MISO
- CS: PE3 (manual GPIO, not hardware NSS)
- UART debug: PA2 = TX, PA3 = RX via USART2 at 115200 baud

## Key Config Decisions

**CTRL_REG4 = 0x67**
ODR bits set to 0110 for 100 Hz output data rate. All three axes enabled by
default (Xen=1, Yen=1, Zen=1). Default ODR is 0000 which is power-down mode -
chip is on but produces no output without this write.

**BDU = 0 (continuous update)**
Output registers update continuously. BDU=1 would hold the registers until both
H and L bytes are read, preventing mismatched reads. Left at 0 for simplicity
here - worth enabling in a library.

**RW bit masking in helpers**
Write helper uses reg & 0x7F to force bit 7 to 0 (RW=0) regardless of what
address is passed in. Read helper uses reg | 0x80 to force bit 7 to 1 (RW=1).
Prevents accidental wrong-direction transactions.

**16-bit reconstruction**
Each axis is split across two 8-bit registers (L and H). H byte is read first,
shifted left 8 bits, then OR'd with L byte to produce a signed int16_t value.

## Output Data

Board flat on table - Z axis reads ~15000, X and Y close to zero.
Tilting the board shifts values between axes as gravity vector moves.
Raw values are in LSB units - sensitivity depends on full-scale setting
in CTRL_REG5 (not configured here, defaults apply).

## Gotchas

- Default ODR in CTRL_REG4 is 0000 = power-down. Chip responds to WHO_AM_I
  but outputs nothing until ODR is set to a non-zero value.
- Each axis needs two SPI reads (L and H registers) - six reads total per sample.
  With software CS toggling this adds overhead. Multi-byte read with ADD_INC
  would be more efficient and is worth using in the library.

## Lessons Learned

- Always check the default register state before assuming the chip is broken -
  CTRL_REG4 defaulting to power-down mode looks like no output, not a bug
- RW bit masking in helpers is a good habit - makes the API harder to misuse
- Z axis reads ~15000 with board flat because gravity is acting on it - expected
  behavior, not a sensor error
