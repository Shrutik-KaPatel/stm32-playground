# SPI_LIS3DSHTR_DMA

## What It Does

Reads live X, Y, Z acceleration data from the LIS3DSHTR using DMA-driven SPI
transfers. DRDY interrupt on INT1/PE0 signals when new data is ready. Instead
of six separate blocking SPI reads, one DMA burst reads all six output registers
in a single transaction. CPU is free during the transfer and gets notified via
callback when done.

## Hardware

- Board: STM32F4DISCOVERY (MB997-F407VGT6-E01)
- Sensor: LIS3DSHTR (U5, onboard MEMS accelerometer)
- SPI1: PA5 = CLK, PA7 = MOSI, PA6 = MISO
- CS: PE3 (manual GPIO)
- INT1/DRDY: PE0 (EXTI0, rising edge trigger)
- UART debug: PA2 = TX, PA3 = RX via USART2 at 115200 baud

## How DMA Works Here

```
DRDY fires on PE0
    -> data_ready flag set in EXTI callback
    -> main loop pulls CS LOW
    -> HAL_SPI_TransmitReceive_DMA called - returns immediately
    -> DMA handles 7 bytes (1 address + 6 data) in background
    -> CPU free during transfer
    -> HAL_SPI_TxRxCpltCallback fires when done
    -> CS HIGH in callback
    -> dma_done flag set
    -> main loop reconstructs XYZ from buffer
```

## Why Multi-Byte Read Works

ADD_INC bit in CTRL_REG6 defaults to 1 - register address auto-increments
after every byte. So sending one address (OUT_X_L = 0x28) and clocking 6
more bytes returns X_L, X_H, Y_L, Y_H, Z_L, Z_H in sequence. Chip keeps
sending until CS goes HIGH.

## Buffer Layout

```c
spi_tx_buf[0] = 0xA8  // RW=1, address=0x28 (OUT_X_L)
spi_tx_buf[1..6] = 0x00  // dummy bytes to clock out data

spi_rx_buf[0] = garbage  // received during address byte, ignored
spi_rx_buf[1] = OUT_X_L
spi_rx_buf[2] = OUT_X_H
spi_rx_buf[3] = OUT_Y_L
spi_rx_buf[4] = OUT_Y_H
spi_rx_buf[5] = OUT_Z_L
spi_rx_buf[6] = OUT_Z_H
```

## Key Config Decisions

**HAL_SPI_TransmitReceive_DMA**
Single function handles both TX and RX simultaneously - SPI is full duplex so
both lines are active at the same time. DMA streams handle each direction
independently (DMA2 Stream0 for RX, DMA2 Stream3 for TX).

**CS control is manual**
CS LOW before DMA call, CS HIGH inside TxRxCpltCallback after transfer done.
Hardware NSS would release CS too early before DMA completes.

**SPI1 global interrupt enabled in NVIC**
Required for DMA transfer complete callbacks to fire. Without it the callback
never gets called.

**Two flags - data_ready and dma_done**
data_ready set by EXTI callback - triggers DMA transfer.
dma_done set by DMA callback - triggers XYZ reconstruction and print.
Keeps ISR minimal and processing in main loop.

## Gotchas

- spi_rx_buf[0] is garbage - data starts at index 1, not 0
- CS must go HIGH inside the DMA callback, not after the DMA call in main -
  the DMA call returns immediately so CS would go HIGH before transfer ends
- SPI1 global interrupt must be enabled alongside DMA interrupts - easy to
  miss since DMA stream interrupts are auto-enabled by CubeMX but SPI global
  is not

## Lessons Learned

- DMA frees the CPU during transfers - return immediately, process in callback
- Multi-byte SPI read with ADD_INC=1 reduces six transactions to one burst
- Two separate flags (trigger and done) keep the data pipeline clean
- CS timing with DMA is different from blocking - must be managed in callback
