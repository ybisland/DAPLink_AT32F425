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
| Target SWCLK | PA5 | GPIO bit-banged SWD |
| Target SWDIO input | PA6 | Separate input from SWDIO output |
| Target SWDIO output | PA7 | Separate output to the target interface |
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

Compile the project with buttons of AT32 VSCode IDE plugin or using cmake commands.

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

## TODO

- Accelerate SWD and SWC with SPI.
