# Bootloader_Jump

Demonstrates how to jump to the STM32 built-in ROM bootloader programmatically
from running code — no BOOT0 pin manipulation needed.

Press B1, the board resets, and STM32CubeProgrammer can connect over USB DFU
to flash new firmware.

---

## What it does

- Polls B1 button (PA0)
- On press: disables interrupts, writes magic flag to RAM, triggers software reset
- On next boot: checks flag before HAL_Init(), jumps to ST ROM bootloader at 0x1FFF0004
- Board appears as a DFU USB device — CubeProgrammer connects and can flash firmware

---

## Hardware

- STM32F4DISCOVERY (STM32F407VGT6)
- CN1 (mini USB) - ST-LINK, power, serial monitor
- CN5 (micro USB) - DFU connection to CubeProgrammer

---

## Key config decisions

**PA0 as GPIO_INPUT not EXTI**
Button is polled in the main loop, not interrupt-driven.
Configured with GPIO_NOPULL — board has external 10k pull-down on PA0.

**USART2 at 115200**
Used for printf debugging via _write syscall redirect.
Confirms boot and button press over serial before the jump.

**bootloader_flag in .noinit section**
Flag must survive a software reset. Placing it in .noinit tells the
linker to skip zeroing this variable at startup — guaranteed persistence.

Added to STM32F407VGTX_FLASH.ld:
```
.noinit (NOLOAD) :
{
    . = ALIGN(4);
    *(.noinit)
    . = ALIGN(4);
} >RAM
```

**Flag check before HAL_Init()**
ROM bootloader expects a clean MCU state — clocks at defaults, SysTick off,
no interrupts. Checking and jumping before HAL_Init() guarantees this since
NVIC_SystemReset() already restored the clean state.

**Jump to address + 4 not address + 0**
0x1FFF0000 holds the initial stack pointer value (data).
0x1FFF0004 holds the Reset Handler address (entry point).
Must jump to +4.

---

## Gotchas

- Original flag storage used _estack - 4 (stack pointer trick from tutorial).
  This did not work — startup code was zeroing that region before the check ran.
  Fixed by using __attribute__((section(".noinit"))) with linker script entry.

- PA0 was configured as GPIO_EXTI0 initially — polling with HAL_GPIO_ReadPin
  does not work in interrupt mode. Changed to GPIO_INPUT.

- CN5 requires a data USB cable, not a charge-only cable.
  Charge-only cables have no data lines — CubeProgrammer will never see the board.

- Linux requires a udev rule for CubeProgrammer to access the DFU device:
```
echo 'SUBSYSTEM=="usb", ATTRS{idVendor}=="0483", ATTRS{idProduct}=="df11", MODE="0666"' \
  | sudo tee /etc/udev/rules.d/49-stm32-dfu.rules
sudo udevadm control --reload-rules
sudo udevadm trigger
```

---

## Lessons learned

**Software reset does not wipe SRAM but startup code might**
NVIC_SystemReset() preserves RAM contents. However, the C runtime startup code
zeros the .bss section on every boot. Variables in .bss or on the stack at
startup time are not safe for flag storage. The .noinit section is the correct
approach — it is explicitly excluded from zeroing by the linker.

**ROM bootloader has a USB timeout**
If nothing connects on USB within a few seconds, the bootloader resets back
to user flash. CubeProgrammer must be open and refreshed quickly after pressing B1.

**udev rules needed for DFU just like ST-LINK**
Linux blocks USB device access without the right udev rules. Same lesson as
ST-LINK setup earlier — any new USB device needs a rules file.

**Jump address is entry point, not base address**
The first 4 bytes at any code region (flash or ROM) are the initial stack pointer,
not executable code. Always jump to base + 4. Jumping to base = jumping into data.

**Two USB ports serve different purposes**
CN1 (mini) = ST-LINK only. CN5 (micro) = OTG/DFU only.
Both needed simultaneously for development with DFU verification.
