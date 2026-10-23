# DAPLink for AT32F425

> ⚠️ This project is in testing phase.
> 
CMSIS-DAP firmware for the Artery AT32F425F8P7. The firmware uses CherryDAP for the debug protocol and CherryUSB for the USB device stack.

## Features

- CMSIS-DAP over USB HID
- Support USB-to-UART and SWO
- SWD only, JTAG and MSC drag-and-drop programming are not supported

## Target and pin assignment

This project uses AT32F425F8P7 (20-pin TSSOP, 64 KiB Flash, 20 KiB SRAM).

| Function | AT32F425 pin | Notes |
| --- | --- | --- |
| USB D- | PA9 | SCFG remaps USB PA11 to PA9 |
| USB D+ | PA10 | SCFG remaps USB PA12 to PA10 |
| Target SWCLK | PA5 | GPIO or SPI1 SCK, depending on the build preset |
| Target SWDIO input | PA6 | Separate input from SWDIO output |
| Target SWDIO output | PA7 | GPIO or SPI1 MOSI; separate from the SWDIO input |
| Target nRESET | PA4 | Controlled by CMSIS-DAP |
| Target SWO input | PA3 | USART2 RX |
| USB-UART TX | PA0 | USART4 TX |
| USB-UART RX | PA1 | USART4 RX |
| Power/firmware LED | PA2 | Active low; turns on after DAP and USB initialization |
| Running LED | PF0 | Active low |
| Connected LED | PF1 | Active low |
| MCU programming SWDIO | PA13 | Programming/debug header |
| MCU programming SWCLK | PA14 | Programming/debug header |

## Build

Open this repository folder with VSCode and AT32 VSCode IDE plugin, then the toolchain will be set up automatically.

Use the following commands to build the firmware:
```sh
cmake --build --preset gpio
cmake --build --preset spi
```

The `gpio` preset sets `DAP_SWD_SPI=0` and writes `build/gpio/DAPLink_AT32F425.hex`.
The `spi` preset sets `DAP_SWD_SPI=1` and writes `build/spi/DAPLink_AT32F425.hex`.

## Details

- SWD enabled and JTAG disabled.
- MSC drag-and-drop programming and the custom HID interface are disabled.
- USART4 is used for USB CDC with circular DMA and main-loop polling.
- SWO UART capture through USART2. SWO streaming transport is disabled.

The AT32F425 has 20 KiB of SRAM, so this project reduces the buffers to:

- CherryDAP UART RX ring buffer: 4 KiB.
- CherryDAP USB RX ring buffer: 4 KiB.
- USART4 RX DMA staging buffer: 2 KiB.
- SWO trace buffer: 4 KiB.

## Validation status

The firmware is under testing.

## Board

The schematic and PCB are open-sourced on [OSHWhub](https://oshwhub.com/totouidc/project_kcqxfiyp).

## SPI 开发情况

CMake 区分了两个构建目标，gpio（普通版本，bit-bang） 和 spi（spi加速版本）。

gpio 版本最高速度10MHz，测试下载速度 ~42.6KiB/s，而spi版本在12MHz下速度为 ~45.5KiB/s，提升不大，因此spi版本暂未启用。

spi 版本有缺陷，

1. spi 速率只有固定的几个，如下：

| 实际速率 | 分频 |
| ---: | ---: |
| 32 MHz | ÷3 |
| 24 MHz | ÷4 |
| 12 MHz | ÷8 |
| 6 MHz | ÷16 |
| 3 MHz | ÷32 |
| 1.5 MHz | ÷64 |
| 750 kHz | ÷128 |
| 375 kHz | ÷256 |
| 187.5 kHz | ÷512 |
| 93.75 kHz | ÷1024 |

F425 最高只能到 32MHz，如果设置 10MHz，实际使用的是 6 MHz，因此速率并不是很灵活。

2. 在 SC32F12T 上进行测试，SPI速度最快只能用 12MHz，24MHz及以上就会报错。

无法确定是Target MCU的SWD速率限制，还是固件问题，还是硬件问题。在这个情况下，SPI最多 12MHz，带来的速度提升并不大。

如果进一步确定，可以使用其他芯片如STM32再进行24MHz的测试。
