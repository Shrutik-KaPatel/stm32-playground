# I2C_Basics

## What This Project Does

Establishes I2C communication with the CS43L22 audio DAC on the STM32F4DISCOVERY board.
The project scans the I2C bus, reads the chip ID register, writes to the Power Control 1
register to power up the audio circuits, then reads it back to confirm the write succeeded.
No audio is produced - this is purely an I2C communication exercise.

## How I Found the Chip and Address

Did not hardcode anything blindly. Followed this procedure from scratch:

**Step 1 - User manual (UM1472)**
Features section mentions "Audio DAC with integrated class D speaker driver" but does
not give the chip name. Manual describes function, not the part number.

**Step 2 - Schematic**
Downloaded from st.com product page under CAD Resources. Found component U7 labelled
CS43L22. Schematic also shows I2C address 0x94 at the bottom of the symbol, SDA on PB9,
SCL on PB6, and RESET on PD4 through a 10K resistor.

**Step 3 - Datasheet**
Googled "CS43L22 datasheet cirrus logic". Found the I2C address section which shows:
- Bits 7-2: fixed as 100101
- Bit 1: AD0 pin (configurable via hardware)
- Bit 0: R/W bit (not part of address)

Checked schematic - A0 pin on U7 is tied to GND, so AD0 = 0.

Full 7-bit address: 1001010 = 0x4A
HAL 8-bit format (shifted left by 1): 0x94

**This procedure works for any I2C device.**

## Hardware

- I2C1: SCL - PB6, SDA - PB9 (remapped from default PB7, schematic confirmed PB9)
- RESET pin: PD4 configured as GPIO Output, must be pulled HIGH before chip responds
- USART2: PA2 TX, PA3 RX for printf over serial

## CubeMX Config

- I2C1: Standard Mode, 100 kHz clock speed, 7-bit addressing
- USART2: Asynchronous, 115200 baud
- PD4: GPIO Output for CS43L22 RESET
- Clock: 168 MHz HCLK

## Key Registers Used

Both found in the CS43L22 datasheet register map:

- **0x01 - ID register (read only):** upper 5 bits are fixed chip ID (11100), lower 3 bits
  are silicon revision. Read back 0xE3 which decodes to CHIPID=11100, REVID=011 (revision B1).

- **0x02 - Power Control 1:** controls power state of internal audio circuits.
  Default is 0x01 (powered down). Wrote 0x9E (1001 1110) to power up. Read back
  confirmed 0x9E.

## Gotchas

- **Default SDA pin is PB7 not PB9.** CubeMX assigns PB7 as I2C1_SDA by default because
  that is the default alternate function in the STM32F407 datasheet. The Discovery board
  PCB trace goes to PB9. Nothing works and no error is thrown - just silence on the bus.
  Always check the schematic, not just CubeMX defaults.

- **CS43L22 will not respond on I2C while in reset.** PD4 defaults LOW after CubeMX
  GPIO output config, which holds the chip in reset. Bus scan returns nothing until PD4
  is pulled HIGH and given a short delay to wake up.

- **HAL uses 8-bit address format.** The 7-bit address 0x4A must be passed as 0x94
  (shifted left by 1) to all HAL I2C functions. The datasheet gives the 7-bit address,
  HAL expects 8-bit. Easy to mix up.

- **0x1A also shows up on the bus scan.** The motion sensor (SPI device) appears to also
  respond on I2C. Not investigated further.

## Lessons Learned

- The user manual identifies chip functions, the schematic identifies part numbers and
  wiring. Both documents are needed - neither alone is enough.

- I2C address derivation always follows the same steps: datasheet for the bit structure,
  schematic for the configurable pin state, then combine. The schematic on this board
  even printed the final address as a convenience but understanding why it is that value
  matters more than memorising it.

- A bus scan is a useful diagnostic tool but not needed when the address is already known.
  Targeting the address directly with HAL_I2C_IsDeviceReady is sufficient for a presence
  check.

- Reading the chip ID register before doing anything else is the embedded equivalent of
  hello world for I2C. It confirms reset pin, I2C pins, address, and read transactions
  all work before touching any functional registers.

- Writing a register and immediately reading it back is the standard way to verify a write
  succeeded. If the read back matches what was written, the full write/read pipeline works.

- HAL_I2C_Mem_Write and HAL_I2C_Mem_Read handle the entire I2C transaction internally
  including START, address, repeated START, ACK handling, and STOP. The register address
  argument (MemAddress) is written first in a write phase before the read or write data.
