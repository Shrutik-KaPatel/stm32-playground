# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/shrutik/esp/esp-idf/components/bootloader/subproject"
  "/home/shrutik/stm32-playground/STM32_to_ESP32_UART_Bridge/esp32_slave/build/bootloader"
  "/home/shrutik/stm32-playground/STM32_to_ESP32_UART_Bridge/esp32_slave/build/bootloader-prefix"
  "/home/shrutik/stm32-playground/STM32_to_ESP32_UART_Bridge/esp32_slave/build/bootloader-prefix/tmp"
  "/home/shrutik/stm32-playground/STM32_to_ESP32_UART_Bridge/esp32_slave/build/bootloader-prefix/src/bootloader-stamp"
  "/home/shrutik/stm32-playground/STM32_to_ESP32_UART_Bridge/esp32_slave/build/bootloader-prefix/src"
  "/home/shrutik/stm32-playground/STM32_to_ESP32_UART_Bridge/esp32_slave/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/shrutik/stm32-playground/STM32_to_ESP32_UART_Bridge/esp32_slave/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/shrutik/stm32-playground/STM32_to_ESP32_UART_Bridge/esp32_slave/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
