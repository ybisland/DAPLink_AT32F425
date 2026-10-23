/*
 * SPI-assisted SWD transfer support for the AT32F425 DAPLink target.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "DAP_config.h"

#if DAP_SWD_SPI

#include "DAP.h"
#include "dap_swd_spi.h"

#define DAP_SWD_SPI_MAX_HZ       36000000U
#define DAP_SWD_SPI_WAIT_LIMIT   200000U
#define DAP_SWD_SPI_PIN_MODE_MASK ((3U << (5U * 2U)) | (3U << (7U * 2U)))

typedef struct {
  uint16_t divider;
  spi_mclk_freq_div_type prescaler;
} dap_swd_spi_prescaler_t;

static const dap_swd_spi_prescaler_t s_prescalers[] = {
  {   2U, SPI_MCLK_DIV_2    },
  {   3U, SPI_MCLK_DIV_3    },
  {   4U, SPI_MCLK_DIV_4    },
  {   8U, SPI_MCLK_DIV_8    },
  {  16U, SPI_MCLK_DIV_16   },
  {  32U, SPI_MCLK_DIV_32   },
  {  64U, SPI_MCLK_DIV_64   },
  { 128U, SPI_MCLK_DIV_128  },
  { 256U, SPI_MCLK_DIV_256  },
  { 512U, SPI_MCLK_DIV_512  },
  {1024U, SPI_MCLK_DIV_1024 }
};

static spi_mclk_freq_div_type s_prescaler = SPI_MCLK_DIV_1024;
static spi_mclk_freq_div_type s_configured_prescaler;
static uint32_t s_spi_clock_hz;
static uint8_t s_clock_available;
static uint8_t s_port_ready;
static uint8_t s_spi_active;
static uint8_t s_spi_configured;
static uint8_t s_frame_bits;

static void DAP_SWD_SPI_SetPinModes(gpio_mode_type mode)
{
  /* PORT_SWD_SETUP has already configured PA5/PA7 as strong push-pull
   * outputs. Only their mode changes for each SPI burst; PA6 stays input. */
  GPIOA->cfgr = (GPIOA->cfgr & ~DAP_SWD_SPI_PIN_MODE_MASK) |
                ((uint32_t)mode << (5U * 2U)) |
                ((uint32_t)mode << (7U * 2U));
}

static void DAP_SWD_SPI_ConfigureTxMux(void)
{
  DAP_SWD_SPI_SetPinModes(GPIO_MODE_MUX);
}

static void DAP_SWD_SPI_ConfigureGPIO(uint32_t last_mosi_bit)
{
  /* Keep SWCLK at its idle-high level across the AF-to-GPIO transition. */
  GPIOA->scr = GPIO_PINS_5;
  if ((last_mosi_bit & 1U) != 0U) {
    GPIOA->scr = GPIO_PINS_7;
  } else {
    GPIOA->clr = GPIO_PINS_7;
  }

  DAP_SWD_SPI_SetPinModes(GPIO_MODE_OUTPUT);
}

static uint8_t DAP_SWD_SPI_WaitFlag(uint32_t flag, flag_status state)
{
  uint32_t timeout = DAP_SWD_SPI_WAIT_LIMIT;

  while (spi_i2s_flag_get(SPI1, flag) != state) {
    if (timeout-- == 0U) {
      return 0U;
    }
  }

  return 1U;
}

static uint8_t DAP_SWD_SPI_Transmit(uint16_t tx_data)
{
  if ((s_spi_active == 0U) ||
      (DAP_SWD_SPI_WaitFlag(SPI_I2S_TDBE_FLAG, SET) == 0U)) {
    return 0U;
  }

  spi_i2s_data_transmit(SPI1, tx_data);
  /* BF can still be clear immediately after writing DT. Wait until the
   * transmit buffer is empty before using BF to detect the final bit. */
  if (DAP_SWD_SPI_WaitFlag(SPI_I2S_TDBE_FLAG, SET) == 0U) {
    return 0U;
  }
  return DAP_SWD_SPI_WaitFlag(SPI_I2S_BF_FLAG, RESET);
}

static uint8_t DAP_SWD_SPI_Transmit32Data(uint32_t tx_data)
{
  if ((s_spi_active == 0U) ||
      (DAP_SWD_SPI_WaitFlag(SPI_I2S_TDBE_FLAG, SET) == 0U)) {
    return 0U;
  }

  spi_i2s_data_transmit(SPI1, (uint16_t)tx_data);
  if (DAP_SWD_SPI_WaitFlag(SPI_I2S_TDBE_FLAG, SET) == 0U) {
    return 0U;
  }
  spi_i2s_data_transmit(SPI1, (uint16_t)(tx_data >> 16));
  if (DAP_SWD_SPI_WaitFlag(SPI_I2S_TDBE_FLAG, SET) == 0U) {
    return 0U;
  }
  return DAP_SWD_SPI_WaitFlag(SPI_I2S_BF_FLAG, RESET);
}

static uint8_t DAP_SWD_SPI_SelectClock(uint32_t requested_hz,
                                       uint32_t *actual_hz,
                                       spi_mclk_freq_div_type *prescaler)
{
  crm_clocks_freq_type clocks;
  uint32_t index;

  if (requested_hz == 0U) {
    return 0U;
  }

  crm_clocks_freq_get(&clocks);
  if (clocks.apb2_freq == 0U) {
    return 0U;
  }

  for (index = 0U; index < (sizeof(s_prescalers) / sizeof(s_prescalers[0])); index++) {
    uint32_t divider = s_prescalers[index].divider;
    uint64_t pclk = clocks.apb2_freq;

    if ((pclk <= ((uint64_t)DAP_SWD_SPI_MAX_HZ * divider)) &&
        (pclk <= ((uint64_t)requested_hz * divider))) {
      *actual_hz = clocks.apb2_freq / divider;
      *prescaler = s_prescalers[index].prescaler;
      return (*actual_hz != 0U) ? 1U : 0U;
    }
  }

  return 0U;
}

static void DAP_SWD_SPI_SetGPIOClockDelay(uint32_t clock_hz)
{
  uint32_t delay;
  uint32_t fast_clock_limit = (CPU_CLOCK / 2U) /
                              (IO_PORT_WRITE_CYCLES + DELAY_FAST_CYCLES);

  if (clock_hz >= fast_clock_limit) {
    DAP_Data.fast_clock = 1U;
    DAP_Data.clock_delay = 1U;
  } else {
    DAP_Data.fast_clock = 0U;
    delay = ((CPU_CLOCK / 2U) + (clock_hz - 1U)) / clock_hz;
    if (delay > IO_PORT_WRITE_CYCLES) {
      delay -= IO_PORT_WRITE_CYCLES;
      delay = (delay + (DELAY_SLOW_CYCLES - 1U)) / DELAY_SLOW_CYCLES;
    } else {
      delay = 1U;
    }
    DAP_Data.clock_delay = delay;
  }
}

/* DAP_SWJ_Clock supplies pyOCD's requested rate. Use the fastest SPI rate
 * which does not exceed it or the AT32F425 SPI clock limit. */
void Set_Clock_Delay(uint32_t clock)
{
  uint32_t gpio_clock = clock;

  s_clock_available = DAP_SWD_SPI_SelectClock(clock, &s_spi_clock_hz, &s_prescaler);
  if (s_clock_available != 0U) {
    gpio_clock = s_spi_clock_hz;
  }

  if (gpio_clock == 0U) {
    gpio_clock = 1U;
  }
  DAP_SWD_SPI_SetGPIOClockDelay(gpio_clock);
}

void DAP_SWD_SPI_Init(void)
{
  if (s_port_ready != 0U) {
    return;
  }

  crm_periph_clock_enable(CRM_SPI1_PERIPH_CLOCK, TRUE);
  spi_i2s_reset(SPI1);
  gpio_pin_mux_config(GPIOA, GPIO_PINS_SOURCE5, GPIO_MUX_0);
  gpio_pin_mux_config(GPIOA, GPIO_PINS_SOURCE7, GPIO_MUX_0);
  s_spi_active = 0U;
  s_spi_configured = 0U;
  s_port_ready = 1U;
}

void DAP_SWD_SPI_Deinit(void)
{
  if (s_spi_active != 0U) {
    DAP_SWD_SPI_End(1U);
  }

  if (s_port_ready != 0U) {
    spi_enable(SPI1, FALSE);
    crm_periph_clock_enable(CRM_SPI1_PERIPH_CLOCK, FALSE);
  }
  s_spi_active = 0U;
  s_spi_configured = 0U;
  s_port_ready = 0U;
}

uint8_t DAP_SWD_SPI_IsAvailable(void)
{
  return ((s_port_ready != 0U) && (s_clock_available != 0U));
}

uint8_t DAP_SWD_SPI_Begin(uint8_t frame_bits)
{
  spi_init_type spi_init_struct;

  if ((DAP_SWD_SPI_IsAvailable() == 0U) || (s_spi_active != 0U) ||
      ((frame_bits != 8U) && (frame_bits != 16U))) {
    return 0U;
  }

  GPIOA->scr = GPIO_PINS_5 | GPIO_PINS_7;

  if ((s_spi_configured == 0U) || (s_configured_prescaler != s_prescaler)) {
    spi_default_para_init(&spi_init_struct);
    /* SWDIO input remains GPIO; the SPI MISO output is never connected. */
    spi_init_struct.transmission_mode = SPI_TRANSMIT_HALF_DUPLEX_TX;
    spi_init_struct.master_slave_mode = SPI_MODE_MASTER;
    spi_init_struct.mclk_freq_division = s_prescaler;
    spi_init_struct.first_bit_transmission = SPI_FIRST_BIT_LSB;
    spi_init_struct.frame_bit_num = (frame_bits == 16U) ? SPI_FRAME_16BIT : SPI_FRAME_8BIT;
    spi_init_struct.clock_polarity = SPI_CLOCK_POLARITY_HIGH;
    spi_init_struct.clock_phase = SPI_CLOCK_PHASE_2EDGE;
    spi_init_struct.cs_mode_selection = SPI_CS_SOFTWARE_MODE;
    spi_init(SPI1, &spi_init_struct);
    spi_i2s_flag_clear(SPI1, SPI_I2S_RDBF_FLAG);
    spi_i2s_flag_clear(SPI1, SPI_I2S_ROERR_FLAG);
    s_configured_prescaler = s_prescaler;
    s_frame_bits = frame_bits;
    s_spi_configured = 1U;
  } else if (s_frame_bits != frame_bits) {
    spi_frame_bit_num_set(SPI1, (frame_bits == 16U) ? SPI_FRAME_16BIT : SPI_FRAME_8BIT);
    s_frame_bits = frame_bits;
  }
  /* Keep PA5 as GPIO high while SPI takes its idle-high state. Switching
   * the pin to AF before programming CLKPOL can create a stray SWCLK edge. */
  spi_enable(SPI1, TRUE);
  DAP_SWD_SPI_ConfigureTxMux();
  s_spi_active = 1U;

  return 1U;
}

uint8_t DAP_SWD_SPI_Transmit8(uint8_t tx_data)
{
  return DAP_SWD_SPI_Transmit(tx_data);
}

uint8_t DAP_SWD_SPI_Transmit32(uint32_t tx_data)
{
  return DAP_SWD_SPI_Transmit32Data(tx_data);
}

void DAP_SWD_SPI_End(uint32_t last_mosi_bit)
{
  (void)DAP_SWD_SPI_WaitFlag(SPI_I2S_TDBE_FLAG, SET);
  (void)DAP_SWD_SPI_WaitFlag(SPI_I2S_BF_FLAG, RESET);
  /* Return SWCLK to the preloaded GPIO-high output before disabling SPI,
   * so the peripheral's disabled state cannot add a falling edge. */
  DAP_SWD_SPI_ConfigureGPIO(last_mosi_bit);
  spi_enable(SPI1, FALSE);
  s_spi_active = 0U;
}

#endif /* DAP_SWD_SPI */
