/* add user code begin Header */
/**
  **************************************************************************
  * @file     at32f425_wk_config.c
  * @brief    work bench config program
  **************************************************************************
  * Copyright (c) 2025, Artery Technology, All rights reserved.
  *
  * The software Board Support Package (BSP) that is made available to
  * download from Artery official website is the copyrighted work of Artery.
  * Artery authorizes customers to use, copy, and distribute the BSP
  * software and its related documentation for the purpose of design and
  * development in conjunction with Artery microcontrollers. Use of the
  * software is governed by this copyright notice and the following disclaimer.
  *
  * THIS SOFTWARE IS PROVIDED ON "AS IS" BASIS WITHOUT WARRANTIES,
  * GUARANTEES OR REPRESENTATIONS OF ANY KIND. ARTERY EXPRESSLY DISCLAIMS,
  * TO THE FULLEST EXTENT PERMITTED BY LAW, ALL EXPRESS, IMPLIED OR
  * STATUTORY OR OTHER WARRANTIES, GUARANTEES OR REPRESENTATIONS,
  * INCLUDING BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY,
  * FITNESS FOR A PARTICULAR PURPOSE, OR NON-INFRINGEMENT.
  *
  **************************************************************************
  */
/* add user code end Header */

#include "at32f425_wk_config.h"

/* private includes ----------------------------------------------------------*/
/* add user code begin private includes */

/* add user code end private includes */

/* private typedef -----------------------------------------------------------*/
/* add user code begin private typedef */

/* add user code end private typedef */

/* private define ------------------------------------------------------------*/
/* add user code begin private define */

/* add user code end private define */

/* private macro -------------------------------------------------------------*/
/* add user code begin private macro */

/* add user code end private macro */

/* private variables ---------------------------------------------------------*/
/* add user code begin private variables */

/* add user code end private variables */

/* private function prototypes --------------------------------------------*/
/* add user code begin function prototypes */

/* add user code end function prototypes */

/* private user code ---------------------------------------------------------*/
/* add user code begin 0 */

/* add user code end 0 */

/**
  * @brief  system clock config program
  * @note   the system clock is configured as follow:
  *         system clock (sclk)   = hick / 12 * pll_mult
  *         system clock source   = HICK_VALUE
  *         - sclk                = 96000000
  *         - ahbdiv              = 1
  *         - ahbclk              = 96000000
  *         - apb1div             = 1
  *         - apb1clk             = 96000000
  *         - apb2div             = 1
  *         - apb2clk             = 96000000
  *         - pll_mult            = 24
  *         - flash_wtcyc         = 2 cycle
  * @param  none
  * @retval none
  */
void wk_system_clock_config(void)
{
  /* reset crm */
  crm_reset();

  /* config flash psr register */
  flash_psr_set(FLASH_WAIT_CYCLE_2);

  /* enable lick */
  crm_clock_source_enable(CRM_CLOCK_SOURCE_LICK, TRUE);

  /* wait till lick is ready */
  while(crm_flag_get(CRM_LICK_STABLE_FLAG) != SET)
  {
  }

  /* enable hick */
  crm_clock_source_enable(CRM_CLOCK_SOURCE_HICK, TRUE);

  /* wait till hick is ready */
  while(crm_flag_get(CRM_HICK_STABLE_FLAG) != SET)
  {
  }

  /* config pll clock resource */
  crm_pll_config(CRM_PLL_SOURCE_HICK, CRM_PLL_MULT_24);

  /* enable pll */
  crm_clock_source_enable(CRM_CLOCK_SOURCE_PLL, TRUE);

  /* wait till pll is ready */
  while(crm_flag_get(CRM_PLL_STABLE_FLAG) != SET)
  {
  }

  /* config ahbclk */
  crm_ahb_div_set(CRM_AHB_DIV_1);

  /* config apb2clk */
  crm_apb2_div_set(CRM_APB2_DIV_1);

  /* config apb1clk */
  crm_apb1_div_set(CRM_APB1_DIV_1);

  /* select pll as system clock source */
  crm_sysclk_switch(CRM_SCLK_PLL);

  /* wait till pll is used as system clock source */
  while(crm_sysclk_switch_status_get() != CRM_SCLK_PLL)
  {
  }

  /* update system_core_clock global variable */
  system_core_clock_update();
}

/**
  * @brief  config periph clock
  * @param  none
  * @retval none
  */
void wk_periph_clock_config(void)
{
  /* enable usb_otgfs1 periph clock */
  crm_periph_clock_enable(CRM_OTGFS1_PERIPH_CLOCK, TRUE);

  /* enable gpioa periph clock */
  crm_periph_clock_enable(CRM_GPIOA_PERIPH_CLOCK, TRUE);

  /* Enable peripherals used by the DAP, USB-UART, and SWO. */
  crm_periph_clock_enable(CRM_GPIOF_PERIPH_CLOCK, TRUE);
  crm_periph_clock_enable(CRM_DMA1_PERIPH_CLOCK, TRUE);
  crm_periph_clock_enable(CRM_USART2_PERIPH_CLOCK, TRUE);
  crm_periph_clock_enable(CRM_USART4_PERIPH_CLOCK, TRUE);

  /* enable scfg periph clock */
  crm_periph_clock_enable(CRM_SCFG_PERIPH_CLOCK, TRUE);

  /* enable acc periph clock */
  crm_periph_clock_enable(CRM_ACC_PERIPH_CLOCK, TRUE);
}

/**
  * @brief  nvic config
  * @param  none
  * @retval none
  */
void wk_nvic_config(void)
{
  nvic_priority_group_config(NVIC_PRIORITY_GROUP_4);

  NVIC_SetPriority(MemoryManagement_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 0, 0));
  NVIC_SetPriority(BusFault_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 0, 0));
  NVIC_SetPriority(UsageFault_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 0, 0));
  NVIC_SetPriority(SVCall_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 0, 0));
  NVIC_SetPriority(DebugMonitor_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 0, 0));
  NVIC_SetPriority(PendSV_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 0, 0));
  NVIC_SetPriority(SysTick_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 15, 0));

  nvic_irq_enable(OTGFS1_IRQn, 0, 0);
  nvic_irq_enable(DMA1_Channel3_2_IRQn, 1, 0);
}

/**
  * @brief  init scfg function.
  * @param  none
  * @retval none
  */
void wk_scfg_init(void)
{
  /* add user code begin scfg_init 0 */

  /* add user code end scfg_init 0 */

  scfg_pa11pa12_pin_remap(SCFG_PA11PA12_TO_PA9PA10);

  scfg_i2s_full_duplex_config(SCFG_FULL_DUPLEX_I2S_NONE);

  /* add user code begin scfg_init 1 */

  /* add user code end scfg_init 1 */
}

/**
  * @brief  init acc function
  * @param  none
  * @retval none
  */
void wk_acc_init(void)
{
  /* add user code begin acc_init 0 */

  /* add user code end acc_init 0 */

  /* update the c1\c2\c3 value */
  acc_write_c1(7980);
  acc_write_c2(8000);
  acc_write_c3(8020);

  /* add user code begin acc_init 1 */

  /* add user code end acc_init 1 */

  /* open acc calibration */
  acc_calibration_mode_enable(ACC_CAL_HICKTRIM, TRUE);

  /* add user code begin acc_init 2 */

  /* add user code end acc_init 2 */
}

/**
  * @brief  init usb_otgfs1 function
  * @param  none
  * @retval none
  */
void wk_usb_otgfs1_init(void)
{
  /* add user code begin usb_otgfs1_init 0 */

  /* add user code end usb_otgfs1_init 0 */
  /* add user code begin usb_otgfs1_init 1 */

  /* add user code end usb_otgfs1_init 1 */

  /* usb divider reset */
  crm_usb_div_reset();

  crm_usb_clock_source_select(CRM_USB_CLOCK_SOURCE_HICK);

  /* add user code begin usb_otgfs1_init 2 */

  /* add user code end usb_otgfs1_init 2 */
}

  /* add user code begin 1 */

static void gpio_config_mux(gpio_type *port,
                            gpio_pins_source_type pin,
                            gpio_mux_sel_type mux,
                            gpio_pull_type pull)
{
  gpio_init_type gpio_init_struct;

  gpio_default_para_init(&gpio_init_struct);
  gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_MODERATE;
  gpio_init_struct.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
  gpio_init_struct.gpio_mode = GPIO_MODE_MUX;
  gpio_init_struct.gpio_pins = (uint32_t)1U << (uint32_t)pin;
  gpio_init_struct.gpio_pull = pull;
  gpio_init(port, &gpio_init_struct);
  gpio_pin_mux_config(port, pin, mux);
}

void wk_gpio_config(void)
{
  gpio_init_type gpio_init_struct;

  /* All three LEDs are active low; preload high so they stay off during setup. */
  gpio_bits_set(GPIOA, GPIO_PINS_2);
  gpio_bits_set(GPIOF, GPIO_PINS_0 | GPIO_PINS_1);
  gpio_default_para_init(&gpio_init_struct);
  gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_MODERATE;
  gpio_init_struct.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
  gpio_init_struct.gpio_mode = GPIO_MODE_OUTPUT;
  gpio_init_struct.gpio_pins = GPIO_PINS_2;
  gpio_init_struct.gpio_pull = GPIO_PULL_NONE;
  gpio_init(GPIOA, &gpio_init_struct);

  gpio_init_struct.gpio_pins = GPIO_PINS_0 | GPIO_PINS_1;
  gpio_init(GPIOF, &gpio_init_struct);
}

void wk_led_power_on(void)
{
  /* LED_PWR is active low; turn it on after DAP and USB initialization. */
  gpio_bits_reset(GPIOA, GPIO_PINS_2);
}

void wk_usart4_init(void)
{
  /* PA0=USART4_TX and PA1=USART4_RX use IOMUX function 4. */
  gpio_config_mux(GPIOA, GPIO_PINS_SOURCE0, GPIO_MUX_4, GPIO_PULL_NONE);
  gpio_config_mux(GPIOA, GPIO_PINS_SOURCE1, GPIO_MUX_4, GPIO_PULL_UP);

  /* Keep the USART disabled until the host supplies CDC line coding. */
  usart_init(USART4, 115200U, USART_DATA_8BITS, USART_STOP_1_BIT);
  usart_parity_selection_config(USART4, USART_PARITY_NONE);
  usart_hardware_flow_control_set(USART4, USART_HARDWARE_FLOW_NONE);
  usart_transmitter_enable(USART4, FALSE);
  usart_receiver_enable(USART4, FALSE);
  usart_dma_transmitter_enable(USART4, FALSE);
  usart_dma_receiver_enable(USART4, FALSE);
  usart_enable(USART4, FALSE);
}

void wk_usart2_swo_init(void)
{
  /* PA3=USART2_RX uses IOMUX function 1. */
  gpio_config_mux(GPIOA, GPIO_PINS_SOURCE3, GPIO_MUX_1, GPIO_PULL_UP);

  /* CherryDAP applies the requested baud rate when SWO UART is enabled. */
  usart_init(USART2, 115200U, USART_DATA_8BITS, USART_STOP_1_BIT);
  usart_parity_selection_config(USART2, USART_PARITY_NONE);
  usart_hardware_flow_control_set(USART2, USART_HARDWARE_FLOW_NONE);
  usart_transmitter_enable(USART2, FALSE);
  usart_receiver_enable(USART2, FALSE);
  usart_dma_receiver_enable(USART2, FALSE);
  usart_enable(USART2, FALSE);
}

void wk_dma_channel_config(dma_channel_type *channel,
                           uint32_t peripheral_base_addr,
                           uint32_t memory_base_addr,
                           uint16_t buffer_size)
{
  dma_init_type dma_init_struct;

  dma_default_para_init(&dma_init_struct);
  dma_init_struct.peripheral_base_addr = peripheral_base_addr;
  dma_init_struct.memory_base_addr = memory_base_addr;
  dma_init_struct.buffer_size = buffer_size;
  dma_init_struct.peripheral_inc_enable = FALSE;
  dma_init_struct.memory_inc_enable = TRUE;
  dma_init_struct.peripheral_data_width = DMA_PERIPHERAL_DATA_WIDTH_BYTE;
  dma_init_struct.memory_data_width = DMA_MEMORY_DATA_WIDTH_BYTE;

  if (channel == DMA1_CHANNEL1) {
    dma_init_struct.direction = DMA_DIR_PERIPHERAL_TO_MEMORY;
    dma_init_struct.loop_mode_enable = TRUE;
    dma_init_struct.priority = DMA_PRIORITY_LOW;
  } else if (channel == DMA1_CHANNEL2) {
    dma_init_struct.direction = DMA_DIR_MEMORY_TO_PERIPHERAL;
    dma_init_struct.loop_mode_enable = FALSE;
    dma_init_struct.priority = DMA_PRIORITY_LOW;
  } else if (channel == DMA1_CHANNEL3) {
    dma_init_struct.direction = DMA_DIR_PERIPHERAL_TO_MEMORY;
    dma_init_struct.loop_mode_enable = FALSE;
    dma_init_struct.priority = DMA_PRIORITY_HIGH;
  } else {
    return;
  }

  dma_init(channel, &dma_init_struct);
}

void wk_dma1_channel1_init(void)
{
  dma_channel_enable(DMA1_CHANNEL1, FALSE);
  dma_reset(DMA1_CHANNEL1);
  dma_flexible_config(DMA1, FLEX_CHANNEL1, DMA_FLEXIBLE_UART4_RX);
  wk_dma_channel_config(DMA1_CHANNEL1, 0U, 0U, 0U);
}

void wk_dma1_channel2_init(void)
{
  dma_channel_enable(DMA1_CHANNEL2, FALSE);
  dma_reset(DMA1_CHANNEL2);
  dma_flexible_config(DMA1, FLEX_CHANNEL2, DMA_FLEXIBLE_UART4_TX);
  wk_dma_channel_config(DMA1_CHANNEL2, 0U, 0U, 0U);
}

void wk_dma1_channel3_init(void)
{
  dma_channel_enable(DMA1_CHANNEL3, FALSE);
  dma_reset(DMA1_CHANNEL3);
  dma_flexible_config(DMA1, FLEX_CHANNEL3, DMA_FLEXIBLE_UART2_RX);
  wk_dma_channel_config(DMA1_CHANNEL3, 0U, 0U, 0U);
}

  /* add user code end 1 */
