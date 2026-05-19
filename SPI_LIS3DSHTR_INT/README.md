# SPI_LIS3DSHTR_INT

## What It Does

Reads live X, Y, Z acceleration data from the LIS3DSHTR using interrupt-driven
data ready signaling. The chip pulses INT1 (PE0) every time new data is ready.
The STM32 catches the rising edge via EXTI0, sets a flag in the ISR, and the
main loop reads XYZ only when the flag is set. No polling, no HAL_Delay.

## Hardware

- Board: STM32F4DISCOVERY (MB997-F407VGT6-E01)
- Sensor: LIS3DSHTR (U5, onboard MEMS accelerometer)
- SPI1: PA5 = CLK, PA7 = MOSI, PA6 = MISO
- CS: PE3 (manual GPIO)
- INT1/DRDY: PE0 (EXTI0, rising edge trigger)
- UART debug: PA2 = TX, PA3 = RX via USART2 at 115200 baud

## How the Interrupt Works

LIS3DSHTR INT1 pin pulses HIGH every time new acceleration data is ready.
PE0 is configured as EXTI0 with rising edge trigger. When the rising edge
hits, the CPU jumps to the ISR which calls HAL_GPIO_EXTI_Callback. The
callback sets a volatile flag. Main loop checks the flag and reads XYZ.

```
Chip has new data
    -> INT1 pulses HIGH
    -> PE0 rising edge detected
    -> EXTI0 IRQ fires
    -> HAL_GPIO_EXTI_Callback called
    -> data_ready = 1
    -> main loop reads XYZ
    -> data_ready = 0
```

## Key Config Decisions

**CTRL_REG3 = 0xE8**
- DR_EN=1 - routes DRDY signal to INT1 pin
- IEA=1 - active HIGH (pin pulses HIGH on data ready)
- IEL=1 - pulsed mode (pin goes back LOW automatically)
- INT1_EN=1 - enables INT1/DRDY signal

Active HIGH chosen because EXTI rising edge trigger is easier to reason about.
Pulsed mode chosen so the pin resets automatically without needing a register read.

**PE0 as GPIO_EXTI0, rising edge**
PE0 maps to EXTI line 0 automatically - pin number determines EXTI line.
Rising edge matches IEA=1 (active HIGH). CubeMX generates the NVIC setup.

**volatile flag pattern**
data_ready declared volatile so the compiler does not optimize away the check
in the main loop. ISR sets it, main loop clears it after reading.

## Gotchas

- Library files must be manually copied into each new project - Save As in
  CubeMX only copies the .ioc file, not hand-written code
- LED toggle inside ISR confirmed interrupt was firing at 100Hz - appeared
  as dim steady glow, not visible individual blinks

## Lessons Learned

- ISR should be minimal - set a flag and return, do the heavy work in main
- volatile is required for any variable shared between ISR and main loop
- LED toggle inside ISR is a fast way to confirm an interrupt is firing
  before debugging software logic
- EXTI line number = pin number - PE0, PA0, PB0 all map to EXTI0, only one
  can be active at a time
- CubeMX generates all NVIC and GPIO config from pinout view - the generated
  MX_GPIO_Init() is where the MCU is told which pin, which edge, which line
