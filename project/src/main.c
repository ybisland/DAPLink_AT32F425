/* add user code begin Header */
/**
  **************************************************************************
  * @file     main.c
  * @brief    main program
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

/* Includes ------------------------------------------------------------------*/
#include "at32f425_wk_config.h"
#include "cdc_usb2uart.h"
#include "dap_main.h"
#include "wk_system.h"

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
  * @brief main function.
  * @param  none
  * @retval none
  */
int main(void)
{
  /* add user code begin 1 */

  /* add user code end 1 */

  /* system clock config. */
  wk_system_clock_config();

  /* config periph clock. */
  wk_periph_clock_config();

  /* init scfg function. */
  wk_scfg_init();

  /* nvic config. */
  wk_nvic_config();

  /* timebase config for
     void wk_delay_us(uint32_t delay);
     void wk_delay_ms(uint32_t delay); */
  wk_timebase_init();

  /* init acc function. */
  wk_acc_init();

  /* init usb_otgfs1 function. */
  wk_usb_otgfs1_init();

  /* Initialize board GPIO, USARTs, and DMA channels. */
  wk_gpio_config();
  wk_usart4_init();
  wk_usart2_swo_init();
  wk_dma1_channel1_init();
  wk_dma1_channel2_init();
  wk_dma1_channel3_init();

  /* Keep USB-UART idle until the host supplies CDC line coding. */
  usb2uart_init();

  /* Register CMSIS-DAP/CDC interfaces and start the USB device. */
  chry_dap_init(0U, OTGFS1_BASE);
  wk_led_power_on();

  /* add user code begin 2 */

  /* add user code end 2 */

  while(1)
  {
    /* add user code begin 3 */

    usb2uart_rx_poll();
    chry_dap_usb2uart_handle();
    chry_dap_handle();

    /* add user code end 3 */
  }
}

/* USB clock, pins, and IRQ are initialized by the board setup before USB starts. */
void usb_dc_low_level_init(uint8_t busid)
{
  (void)busid;
}

void usb_dc_low_level_deinit(uint8_t busid)
{
  (void)busid;
}

  /* add user code begin 4 */

  /* add user code end 4 */
