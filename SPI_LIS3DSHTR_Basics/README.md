# SPI_LIS3DSHTR_Basics

## What It Does

Reads the WHO_AM_I register (0x0F) from the LIS3DSHTR MEMS accelerometer over SPI
and prints the result over UART. Confirms SPI communication is working before building
anything on top of it. Expected value is 0x3F.

## Hardware

- Board: STM32F4DISCOVERY (MB997-F407VGT6-E01)
- Sensor: LIS3DSHTR (U5, onboard MEMS accelerometer)
- SPI1: PA5 = CLK, PA7 = MOSI, PA6 = MISO
- CS: PE3 (manual GPIO, not hardware NSS)
- INT1: PE0, INT2: PE1 (not used in this project)
- UART debug: PA2 = TX, PA3 = RX via USART2 at 115200 baud

## How the Chip and Pins Were Found

- User manual (UM1472) - Table 7 lists motion sensor pins, confirms LIS3DSHTR is U5
- Schematic - traced SPI pins to PA5/PA6/PA7, CS on PE3 with no pull resistor,
  INT1 on PE0, INT2 on PE1, chip powered at 3V with 100nF decoupling cap
- Datasheet - confirmed SPI Mode 3 (CPOL=1, CPHA=1), max clock 10 MHz,
  WHO_AM_I at address 0x0F returns 0x3F, RW bit is bit 0 of the address byte

## Key Config Decisions

**SPI Mode 3 (CPOL=1, CPHA=1)**
Datasheet states clock idles high and data is driven on falling edge, captured
on rising edge - that maps to CPOL=1, CPHA=1. The datasheet doesn't say
"Mode 3" explicitly, it has to be read from the timing description.

**Prescaler 16 - 5.25 MHz clock**
SPI1 is on APB2 at 84 MHz. Max SPI clock on LIS3DSHTR is 10 MHz per datasheet.
Prescaler 8 gives 10.5 MHz which is just over limit - prescaler 16 gives
5.25 MHz which is safely under.

**Manual CS via GPIO**
PE3 configured as GPIO Output instead of hardware NSS. Gives explicit control
over CS timing, which is the standard approach for sensor drivers and required
for the library pattern used in the next project.

**CS idles HIGH**
LIS3DSHTR uses CS to select protocol - CS HIGH enables I2C mode, CS LOW enables
SPI mode. Must drive CS HIGH at startup before any transaction or the chip
boots in an undefined state.

**SPI read frame format**
A read is two bytes: first byte is 1 | address (RW=1), second byte is a dummy
0x00 to clock out the response. HAL_SPI_Transmit then HAL_SPI_Receive as
separate calls works but shows as two frames on the logic analyzer.

## Gotchas

- Code placed in USER CODE BEGIN 1 initially - that section runs before HAL_Init()
  and all MX_ peripheral inits, so SPI isn't initialized yet. Everything must
  go in USER CODE BEGIN 2.
- _write() syscall defined inside main() - C does not allow nested functions.
  Must be in USER CODE BEGIN 4 which is outside main().
- CS macros had semicolons inside the #define - causes subtle bugs when used
  inside if statements. Never put semicolons inside macro definitions.
- Logic analyzer sample rate was 8 MHz initially - at 5.25 MHz SPI clock that
  is not enough oversampling, causing the decoder to read garbage bits regardless
  of CPOL/CPHA settings. Increasing to 16 MHz fixed the decoding.
- Logic analyzer CS probe was not connected initially - without CS the decoder
  cannot frame transactions correctly and misaligns the bits.

## Lessons Learned

- Always trace SPI/I2C pins from schematic before CubeMX - user manual gives
  function names, schematic gives actual pin connections and pull resistor info
- SPI has no built-in read/write distinction unlike I2C - the RW bit in the
  address byte is how the chip knows what you want
- CPOL/CPHA must be read from the timing description in the datasheet, not just
  looked up - the LIS3DSHTR datasheet never says "Mode 3" but the timing diagram
  makes it clear
- Logic analyzer sample rate is not optional - 4x oversampling minimum relative
  to SPI clock. Too low and the decoder produces garbage regardless of settings
- CS must be connected to the logic analyzer decoder - without it the decoder
  cannot identify transaction boundaries and misreads bit alignment
