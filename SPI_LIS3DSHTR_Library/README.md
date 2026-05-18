# SPI_LIS3DSHTR_Library

## What It Does

Portable SPI driver library for the LIS3DSHTR MEMS accelerometer. Wraps register
read/write over SPI into a clean handle-based API so the driver is not hardcoded
to any specific SPI peripheral or CS pin. Main application just initializes a
handle and calls ReadXYZ in a loop.

## Hardware

- Board: STM32F4DISCOVERY (MB997-F407VGT6-E01)
- Sensor: LIS3DSHTR (U5, onboard MEMS accelerometer)
- SPI1: PA5 = CLK, PA7 = MOSI, PA6 = MISO
- CS: PE3 (manual GPIO)
- UART debug: PA2 = TX, PA3 = RX via USART2 at 115200 baud

## Library Structure

```
LIS3DSHTR/
    LIS3DSHTR.h   - register defines, handle struct, data struct, public API
    LIS3DSHTR.c   - internal helpers (WriteReg, ReadReg), Init, ReadXYZ
```

### Handle Struct
```c
typedef struct {
    SPI_HandleTypeDef *hspi;    // which SPI peripheral
    GPIO_TypeDef      *cs_port; // CS GPIO port
    uint16_t           cs_pin;  // CS GPIO pin
} LIS3_HandleTypeDef;
```
Members derived from HAL function arguments - HAL_SPI_Transmit needs hspi,
HAL_GPIO_WritePin needs cs_port and cs_pin. Nothing hardcoded.

### Public API
```c
void LIS3_Init(LIS3_HandleTypeDef *hlis);
void LIS3_ReadXYZ(LIS3_HandleTypeDef *hlis, LIS3_DataTypeDef *data);
```

### Internal Helpers (not exposed in header)
```c
void    LIS3_WriteReg(LIS3_HandleTypeDef *hlis, uint8_t reg, uint8_t data);
uint8_t LIS3_ReadReg(LIS3_HandleTypeDef *hlis, uint8_t reg);
```

## Key Config Decisions

**RW bit masking**
WriteReg uses reg & 0x7F to force bit 7 to 0 (write). ReadReg uses reg | 0x80
to force bit 7 to 1 (read). Safe regardless of what address is passed in.

**CTRL_REG4 = 0x67**
ODR=100Hz, all axes enabled. Defined as LIS3_CTRL_REG4_100HZ_XYZ in header
instead of hardcoded magic number.

**Struct members derived from HAL signatures**
To find what goes in the struct - look at what every HAL transaction needs as
arguments. Those argument types become the struct members.

## Gotchas

- Custom folders created outside Core/Src are excluded from build by default
  in CubeIDE. Right-click folder → Properties → C/C++ Build → uncheck
  "Exclude resource from build". Include path alone is not enough - that only
  finds headers, it does not compile source files.
- Include path and source compilation are two separate settings in CubeIDE.
  Both must be correct for a custom folder library to work.

## Lessons Learned

- Handle/struct pattern makes drivers portable - swap hspi1 for hspi2 and
  the library works on a different peripheral with zero code changes
- Internal helpers belong in the .c file only, not the header - keeps the
  public API minimal and prevents misuse
- CubeIDE build system needs explicit inclusion of custom source folders -
  it does not automatically compile everything in the project tree
