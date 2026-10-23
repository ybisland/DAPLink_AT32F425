/*
 * Copyright (c) 2013-2017 ARM Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the License); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * ----------------------------------------------------------------------
 *
 * $Date:        1. December 2017
 * $Revision:    V2.0.0
 *
 * Project:      CMSIS-DAP Source
 * Title:        SW_DP.c CMSIS-DAP SW DP I/O
 *
 *---------------------------------------------------------------------------*/

#include "DAP_config.h"
#include "DAP.h"


// SW Macros

#define PIN_SWCLK_SET PIN_SWCLK_TCK_SET
#define PIN_SWCLK_CLR PIN_SWCLK_TCK_CLR

#define SW_CLOCK_CYCLE()                \
  PIN_SWCLK_CLR();                      \
  PIN_DELAY();                          \
  PIN_SWCLK_SET();                      \
  PIN_DELAY()

#define SW_WRITE_BIT(bit)               \
  PIN_SWDIO_OUT(bit);                   \
  PIN_SWCLK_CLR();                      \
  PIN_DELAY();                          \
  PIN_SWCLK_SET();                      \
  PIN_DELAY()

#define SW_READ_BIT(bit)                \
  PIN_SWCLK_CLR();                      \
  PIN_DELAY();                          \
  bit = PIN_SWDIO_IN();                 \
  PIN_SWCLK_SET();                      \
  PIN_DELAY()

#define PIN_DELAY() PIN_DELAY_SLOW(DAP_Data.clock_delay)


// Generate SWJ Sequence
//   count:  sequence bit count
//   data:   pointer to sequence bit data
//   return: none
#if ((DAP_SWD != 0) || (DAP_JTAG != 0))
void SWJ_Sequence (uint32_t count, const uint8_t *data) {
  uint32_t val;
  uint32_t n;

  val = 0U;
  n = 0U;
  while (count--) {
    if (n == 0U) {
      val = *data++;
      n = 8U;
    }
    if (val & 1U) {
      PIN_SWDIO_TMS_SET();
    } else {
      PIN_SWDIO_TMS_CLR();
    }
    SW_CLOCK_CYCLE();
    val >>= 1;
    n--;
  }
}
#endif


// Generate SWD Sequence
//   info:   sequence information
//   swdo:   pointer to SWDIO generated data
//   swdi:   pointer to SWDIO captured data
//   return: none
#if (DAP_SWD != 0)
void SWD_Sequence (uint32_t info, const uint8_t *swdo, uint8_t *swdi) {
  uint32_t val;
  uint32_t bit;
  uint32_t n, k;

  n = info & SWD_SEQUENCE_CLK;
  if (n == 0U) {
    n = 64U;
  }

  if (info & SWD_SEQUENCE_DIN) {
    while (n) {
      val = 0U;
      for (k = 8U; k && n; k--, n--) {
        SW_READ_BIT(bit);
        val >>= 1;
        val  |= bit << 7;
      }
      val >>= k;
      *swdi++ = (uint8_t)val;
    }
  } else {
    while (n) {
      val = *swdo++;
      for (k = 8U; k && n; k--, n--) {
        SW_WRITE_BIT(val);
        val >>= 1;
      }
    }
  }
}
#endif


#if (DAP_SWD != 0)


// SWD Transfer I/O
//   request: A[3:2] RnW APnDP
//   data:    DATA[31:0]
//   return:  ACK[2:0]
#define SWD_TransferFunction(speed)     /**/                                    \
static uint8_t SWD_Transfer##speed (uint32_t request, uint32_t *data) {         \
  uint32_t ack;                                                                 \
  uint32_t bit;                                                                 \
  uint32_t val;                                                                 \
  uint32_t parity;                                                              \
                                                                                \
  uint32_t n;                                                                   \
                                                                                \
  /* Packet Request */                                                          \
  parity = 0U;                                                                  \
  SW_WRITE_BIT(1U);                     /* Start Bit */                         \
  bit = request >> 0;                                                           \
  SW_WRITE_BIT(bit);                    /* APnDP Bit */                         \
  parity += bit;                                                                \
  bit = request >> 1;                                                           \
  SW_WRITE_BIT(bit);                    /* RnW Bit */                           \
  parity += bit;                                                                \
  bit = request >> 2;                                                           \
  SW_WRITE_BIT(bit);                    /* A2 Bit */                            \
  parity += bit;                                                                \
  bit = request >> 3;                                                           \
  SW_WRITE_BIT(bit);                    /* A3 Bit */                            \
  parity += bit;                                                                \
  SW_WRITE_BIT(parity);                 /* Parity Bit */                        \
  SW_WRITE_BIT(0U);                     /* Stop Bit */                          \
  SW_WRITE_BIT(1U);                     /* Park Bit */                          \
                                                                                \
  /* Turnaround */                                                              \
  PIN_SWDIO_OUT_DISABLE();                                                      \
  for (n = DAP_Data.swd_conf.turnaround; n; n--) {                              \
    SW_CLOCK_CYCLE();                                                           \
  }                                                                             \
                                                                                \
  /* Acknowledge response */                                                    \
  SW_READ_BIT(bit);                                                             \
  ack  = bit << 0;                                                              \
  SW_READ_BIT(bit);                                                             \
  ack |= bit << 1;                                                              \
  SW_READ_BIT(bit);                                                             \
  ack |= bit << 2;                                                              \
                                                                                \
  if (ack == DAP_TRANSFER_OK) {         /* OK response */                       \
    /* Data transfer */                                                         \
    if (request & DAP_TRANSFER_RnW) {                                           \
      /* Read data */                                                           \
      val = 0U;                                                                 \
      parity = 0U;                                                              \
      for (n = 32U; n; n--) {                                                   \
        SW_READ_BIT(bit);               /* Read RDATA[0:31] */                  \
        parity += bit;                                                          \
        val >>= 1;                                                              \
        val  |= bit << 31;                                                      \
      }                                                                         \
      SW_READ_BIT(bit);                 /* Read Parity */                       \
      if ((parity ^ bit) & 1U) {                                                \
        ack = DAP_TRANSFER_ERROR;                                               \
      }                                                                         \
      if (data) { *data = val; }                                                \
      /* Turnaround */                                                          \
      for (n = DAP_Data.swd_conf.turnaround; n; n--) {                          \
        SW_CLOCK_CYCLE();                                                       \
      }                                                                         \
      PIN_SWDIO_OUT_ENABLE();                                                   \
    } else {                                                                    \
      /* Turnaround */                                                          \
      for (n = DAP_Data.swd_conf.turnaround; n; n--) {                          \
        SW_CLOCK_CYCLE();                                                       \
      }                                                                         \
      PIN_SWDIO_OUT_ENABLE();                                                   \
      /* Write data */                                                          \
      val = *data;                                                              \
      parity = 0U;                                                              \
      for (n = 32U; n; n--) {                                                   \
        SW_WRITE_BIT(val);              /* Write WDATA[0:31] */                 \
        parity += val;                                                          \
        val >>= 1;                                                              \
      }                                                                         \
      SW_WRITE_BIT(parity);             /* Write Parity Bit */                  \
    }                                                                           \
    /* Capture Timestamp */                                                     \
    if (request & DAP_TRANSFER_TIMESTAMP) {                                     \
      DAP_Data.timestamp = TIMESTAMP_GET();                                     \
    }                                                                           \
    /* Idle cycles */                                                           \
    n = DAP_Data.transfer.idle_cycles;                                          \
    if (n) {                                                                    \
      PIN_SWDIO_OUT(0U);                                                        \
      for (; n; n--) {                                                          \
        SW_CLOCK_CYCLE();                                                       \
      }                                                                         \
    }                                                                           \
    PIN_SWDIO_OUT(1U);                                                          \
    return ((uint8_t)ack);                                                      \
  }                                                                             \
                                                                                \
  if ((ack == DAP_TRANSFER_WAIT) || (ack == DAP_TRANSFER_FAULT)) {              \
    /* WAIT or FAULT response */                                                \
    if (DAP_Data.swd_conf.data_phase && ((request & DAP_TRANSFER_RnW) != 0U)) { \
      for (n = 32U+1U; n; n--) {                                                \
        SW_CLOCK_CYCLE();               /* Dummy Read RDATA[0:31] + Parity */   \
      }                                                                         \
    }                                                                           \
    /* Turnaround */                                                            \
    for (n = DAP_Data.swd_conf.turnaround; n; n--) {                            \
      SW_CLOCK_CYCLE();                                                         \
    }                                                                           \
    PIN_SWDIO_OUT_ENABLE();                                                     \
    if (DAP_Data.swd_conf.data_phase && ((request & DAP_TRANSFER_RnW) == 0U)) { \
      PIN_SWDIO_OUT(0U);                                                        \
      for (n = 32U+1U; n; n--) {                                                \
        SW_CLOCK_CYCLE();               /* Dummy Write WDATA[0:31] + Parity */  \
      }                                                                         \
    }                                                                           \
    PIN_SWDIO_OUT(1U);                                                          \
    return ((uint8_t)ack);                                                      \
  }                                                                             \
                                                                                \
  /* Protocol error */                                                          \
  for (n = DAP_Data.swd_conf.turnaround + 32U + 1U; n; n--) {                   \
    SW_CLOCK_CYCLE();                   /* Back off data phase */               \
  }                                                                             \
  PIN_SWDIO_OUT_ENABLE();                                                       \
  PIN_SWDIO_OUT(1U);                                                            \
  return ((uint8_t)ack);                                                        \
}


#undef  PIN_DELAY
#define PIN_DELAY() PIN_DELAY_FAST()
SWD_TransferFunction(Fast)

#undef  PIN_DELAY
#define PIN_DELAY() PIN_DELAY_SLOW(DAP_Data.clock_delay)
SWD_TransferFunction(Slow)

#if DAP_SWD_SPI
/* GPIO cycles are used for SWD turnaround, ACK, parity, idle clocks, and
 * WAIT/FAULT data phases. Match their delay to the selected SPI rate. */
__STATIC_FORCEINLINE void SWD_SPI_GPIO_Delay(void) {
  if (DAP_Data.fast_clock) {
    PIN_DELAY_FAST();
  } else {
    PIN_DELAY_SLOW(DAP_Data.clock_delay);
  }
}

__STATIC_FORCEINLINE void SWD_SPI_GPIO_ClockCycle(void) {
  PIN_SWCLK_CLR();
  SWD_SPI_GPIO_Delay();
  PIN_SWCLK_SET();
  SWD_SPI_GPIO_Delay();
}

__STATIC_FORCEINLINE void SWD_SPI_GPIO_WriteBit(uint32_t bit) {
  PIN_SWDIO_OUT(bit);
  SWD_SPI_GPIO_ClockCycle();
}

__STATIC_FORCEINLINE uint32_t SWD_SPI_GPIO_ReadBit(void) {
  uint32_t bit;
  PIN_SWCLK_CLR();
  SWD_SPI_GPIO_Delay();
  bit = PIN_SWDIO_IN();
  PIN_SWCLK_SET();
  SWD_SPI_GPIO_Delay();
  return bit;
}

static uint8_t SWD_TransferSPI(uint32_t request, uint32_t *data) {
  uint32_t ack;
  uint32_t bit;
  uint32_t val;
  uint32_t parity;
  uint32_t n;
  uint8_t request_byte;

  if (DAP_SWD_SPI_IsAvailable() == 0U) {
    return DAP_Data.fast_clock ? SWD_TransferFast(request, data)
                               : SWD_TransferSlow(request, data);
  }

  /* SWD request bits are sent LSB first: start, request[3:0], parity,
   * stop, park. SPI is configured LSB-first in mode 3. */
  parity = request & 0x0FU;
  parity ^= parity >> 2;
  parity ^= parity >> 1;
  request_byte = (uint8_t)(0x81U | ((request & 0x0FU) << 1) |
                           ((parity & 1U) << 5));

  if (DAP_SWD_SPI_Begin(8U) == 0U) {
    return DAP_TRANSFER_ERROR;
  }
  if (DAP_SWD_SPI_Transmit8(request_byte) == 0U) {
    DAP_SWD_SPI_End(1U, 0U);
    return DAP_TRANSFER_ERROR;
  }
  DAP_SWD_SPI_End(1U, 1U);

  /* Turnaround and ACK are sampled with GPIO so SWDIO can change direction. */
  PIN_SWDIO_OUT_DISABLE();
  for (n = DAP_Data.swd_conf.turnaround; n; n--) {
    SWD_SPI_GPIO_ClockCycle();
  }

  bit = SWD_SPI_GPIO_ReadBit();
  ack = bit;
  bit = SWD_SPI_GPIO_ReadBit();
  ack |= bit << 1;
  bit = SWD_SPI_GPIO_ReadBit();
  ack |= bit << 2;

  if (ack == DAP_TRANSFER_OK) {
    if (request & DAP_TRANSFER_RnW) {
      val = 0U;
      parity = 0U;

      /* Target owns SWDIO during reads. Keep this phase on the known-good
       * GPIO path instead of using SPI MISO and MOSI at the same time. */
      for (n = 32U; n; n--) {
        bit = SWD_SPI_GPIO_ReadBit();
        parity += bit;
        val >>= 1;
        val |= bit << 31;
      }

      bit = SWD_SPI_GPIO_ReadBit();
      if ((parity ^ bit) & 1U) {
        ack = DAP_TRANSFER_ERROR;
      }
      if (data) {
        *data = val;
      }

      for (n = DAP_Data.swd_conf.turnaround; n; n--) {
        SWD_SPI_GPIO_ClockCycle();
      }
      PIN_SWDIO_OUT_ENABLE();
    } else {
      for (n = DAP_Data.swd_conf.turnaround; n; n--) {
        SWD_SPI_GPIO_ClockCycle();
      }
      PIN_SWDIO_OUT_ENABLE();

      val = *data;
      parity = val;
      parity ^= parity >> 16;
      parity ^= parity >> 8;
      parity ^= parity >> 4;
      parity ^= parity >> 2;
      parity ^= parity >> 1;
      parity &= 1U;

      if (DAP_SWD_SPI_Begin(16U) != 0U) {
        if (DAP_SWD_SPI_Transmit32(val) == 0U) {
          DAP_SWD_SPI_End((val >> 31) & 1U, 0U);
          return DAP_TRANSFER_ERROR;
        }
        DAP_SWD_SPI_End((val >> 31) & 1U, 1U);
      } else {
        for (n = 32U; n; n--) {
          SWD_SPI_GPIO_WriteBit(val);
          val >>= 1;
        }
      }
      SWD_SPI_GPIO_WriteBit(parity);
    }

    if (request & DAP_TRANSFER_TIMESTAMP) {
      DAP_Data.timestamp = TIMESTAMP_GET();
    }

    n = DAP_Data.transfer.idle_cycles;
    if (n) {
      PIN_SWDIO_OUT(0U);
      for (; n; n--) {
        SWD_SPI_GPIO_ClockCycle();
      }
    }
    PIN_SWDIO_OUT(1U);
    return (uint8_t)ack;
  }

  if ((ack == DAP_TRANSFER_WAIT) || (ack == DAP_TRANSFER_FAULT)) {
    if (DAP_Data.swd_conf.data_phase && ((request & DAP_TRANSFER_RnW) != 0U)) {
      for (n = 32U + 1U; n; n--) {
        SWD_SPI_GPIO_ClockCycle();
      }
    }
    for (n = DAP_Data.swd_conf.turnaround; n; n--) {
      SWD_SPI_GPIO_ClockCycle();
    }
    PIN_SWDIO_OUT_ENABLE();
    if (DAP_Data.swd_conf.data_phase && ((request & DAP_TRANSFER_RnW) == 0U)) {
      PIN_SWDIO_OUT(0U);
      for (n = 32U + 1U; n; n--) {
        SWD_SPI_GPIO_ClockCycle();
      }
    }
    PIN_SWDIO_OUT(1U);
    return (uint8_t)ack;
  }

  for (n = DAP_Data.swd_conf.turnaround + 32U + 1U; n; n--) {
    SWD_SPI_GPIO_ClockCycle();
  }
  PIN_SWDIO_OUT_ENABLE();
  PIN_SWDIO_OUT(1U);
  return (uint8_t)ack;
}
#endif


// SWD Transfer I/O
//   request: A[3:2] RnW APnDP
//   data:    DATA[31:0]
//   return:  ACK[2:0]
uint8_t  SWD_Transfer(uint32_t request, uint32_t *data) {
#if DAP_SWD_SPI
  return SWD_TransferSPI(request, data);
#else
  if (DAP_Data.fast_clock) {
    return SWD_TransferFast(request, data);
  } else {
    return SWD_TransferSlow(request, data);
  }
#endif
}


#endif  /* (DAP_SWD != 0) */
