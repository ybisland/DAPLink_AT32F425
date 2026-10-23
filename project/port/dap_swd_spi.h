/*
 * SPI-assisted SWD transfer support for the AT32F425 DAPLink target.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef DAP_SWD_SPI_H
#define DAP_SWD_SPI_H

#include <stdint.h>

void DAP_SWD_SPI_Init(void);
void DAP_SWD_SPI_Deinit(void);
uint8_t DAP_SWD_SPI_IsAvailable(void);
uint8_t DAP_SWD_SPI_Begin(uint8_t frame_bits);
uint8_t DAP_SWD_SPI_Transmit8(uint8_t tx_data);
uint8_t DAP_SWD_SPI_Transmit32(uint32_t tx_data);
void DAP_SWD_SPI_End(uint32_t last_mosi_bit, uint8_t transfer_complete);

#endif
