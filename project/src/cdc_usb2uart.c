/*
 * Polling-based USART4 <-> USB CDC bridge for AT32F425.
 *
 * RX uses a circular DMA staging buffer. usb2uart_rx_poll() polls the DMA
 * write position and copies newly received bytes into CherryDAP's g_uartrx.
 * No USART IDLE, DMA half-transfer, or DMA transfer-complete RX interrupt is
 * used by this implementation. So poll well before one full DMA-buffer
 * time elapses (2048 8N1 bytes take about 20 ms at 1M baud). If DMA makes a
 * complete lap between polls, the producer position alone cannot tell that
 * bytes were overwritten.
 *
 * A valid CDC line-coding update aborts the active TX DMA block and restarts RX
 * DMA without forwarding bytes still in its staging buffer.
 */

#include "at32f425_wk_config.h"
#include "cdc_usb2uart.h"
#include "dap_main.h"
#include <stdbool.h>

#define UART_RX_DMA_BUFFER_SIZE 2048U
#define UART_BAUD_DIV_MIN       16U
#define UART_BAUD_DIV_MAX       65535U

static bool uart_configured;
static uint8_t uart_rx_dma_buffer[UART_RX_DMA_BUFFER_SIZE];
static uint32_t uart_rx_last_pos;
static volatile uint32_t uart_tx_active_length;

void usb2uart_init(void)
{
    /* Keep the bridge idle until valid CDC line coding is received.
     *
     * The USART and DMA should be configured to idle state in wk_xxx_init(),
     * here we just to make sure the USART and DMA are disabled.
     */
    usart_enable(USART4, FALSE);
    usart_transmitter_enable(USART4, FALSE);
    usart_receiver_enable(USART4, FALSE);
    usart_dma_transmitter_enable(USART4, FALSE);
    usart_dma_receiver_enable(USART4, FALSE);

    dma_channel_enable(DMA1_CHANNEL1, FALSE);
    dma_channel_enable(DMA1_CHANNEL2, FALSE);
    dma_interrupt_enable(DMA1_CHANNEL1,
                         DMA_FDT_INT | DMA_HDT_INT | DMA_DTERR_INT,
                         FALSE);
    dma_interrupt_enable(DMA1_CHANNEL2,
                         DMA_FDT_INT | DMA_HDT_INT | DMA_DTERR_INT,
                         FALSE);
    dma_flag_clear(DMA1_FDT1_FLAG | DMA1_HDT1_FLAG | DMA1_DTERR1_FLAG |
                   DMA1_FDT2_FLAG | DMA1_HDT2_FLAG | DMA1_DTERR2_FLAG);

    uart_configured = false;
    uart_rx_last_pos = 0U;
    uart_tx_active_length = 0U;

    dma_interrupt_enable(DMA1_CHANNEL2, DMA_FDT_INT, TRUE);
    nvic_irq_enable(DMA1_Channel3_2_IRQn, 1, 0);
}

/* -------------------------------------------------------------------------- */
/*                               CDC Line Config                              */
/* -------------------------------------------------------------------------- */
typedef struct {
    uint32_t baudrate;
    usart_data_bit_num_type data_bits;
    usart_stop_bit_num_type stop_bits;
    usart_parity_selection_type parity;
} usb2uart_line_config_t;

static bool uart_decode_line_config(const struct cdc_line_coding *line_coding,
                                    usb2uart_line_config_t *config)
{
    crm_clocks_freq_type clocks;
    uint32_t min_baudrate;
    uint32_t max_baudrate;

    if ((line_coding == NULL) || (config == NULL) ||
        (line_coding->dwDTERate == 0U)) {
        return false;
    }

    /* Only support 8 data bits */
    switch (line_coding->bDataBits) {
    case 8U:
        config->data_bits = USART_DATA_8BITS;
        break;
    default:
        return false;
    }

    /* Check if the requested baudrate is within the peripheral valid range. */
    crm_clocks_freq_get(&clocks);
    if (clocks.apb1_freq == 0U) {
        return false;
    }
    min_baudrate =
        (clocks.apb1_freq + UART_BAUD_DIV_MAX - 1U) / UART_BAUD_DIV_MAX;
    max_baudrate = clocks.apb1_freq / UART_BAUD_DIV_MIN;
    if ((line_coding->dwDTERate < min_baudrate) ||
        (line_coding->dwDTERate > max_baudrate)) {
        return false;
    }
    config->baudrate = line_coding->dwDTERate;

    switch (line_coding->bCharFormat) {
    case 0U:
        config->stop_bits = USART_STOP_1_BIT;
        break;
    case 1U:
        config->stop_bits = USART_STOP_1_5_BIT;
        break;
    case 2U:
        config->stop_bits = USART_STOP_2_BIT;
        break;
    default:
        return false;
    }

    switch (line_coding->bParityType) {
    case 0U:
        config->parity = USART_PARITY_NONE;
        config->data_bits = USART_DATA_8BITS;
        break;
    case 1U:
        config->parity = USART_PARITY_ODD;
        config->data_bits = USART_DATA_9BITS;
        break;
    case 2U:
        config->parity = USART_PARITY_EVEN;
        config->data_bits = USART_DATA_9BITS;
        break;
    default:
        /* AT32F425 USART does not support CDC Mark/Space parity modes. */
        return false;
    }

    return true;
}

static void uart_rx_dma_restart(void)
{
    dma_channel_enable(DMA1_CHANNEL1, FALSE);
    dma_flag_clear(DMA1_FDT1_FLAG);

    wk_dma_channel_config(DMA1_CHANNEL1,
                          (uint32_t)&USART4->dt,
                          (uint32_t)uart_rx_dma_buffer,
                          UART_RX_DMA_BUFFER_SIZE);

    uart_rx_last_pos = 0U;
    dma_channel_enable(DMA1_CHANNEL1, TRUE);
}

static void uart_apply_line_config(const usb2uart_line_config_t *config)
{
    uint32_t aborted_tx_length;

    uart_configured = false;

    /* Stop both directions. Bytes still in the RX DMA buffer are discarded. */
    dma_channel_enable(DMA1_CHANNEL2, FALSE);
    dma_channel_enable(DMA1_CHANNEL1, FALSE);
    dma_flag_clear(DMA1_FDT2_FLAG);

    usart_enable(USART4, FALSE);

    /* Discard a byte already in the USART receive register as part of RX abort. */
    if (usart_flag_get(USART4, USART_RDBF_FLAG) != RESET) {
        (void)usart_data_receive(USART4);
    }

    /* Discard any pending TX data. */
    aborted_tx_length = uart_tx_active_length;
    uart_tx_active_length = 0U;
    if (aborted_tx_length != 0U) {
        chry_dap_usb2uart_uart_send_complete(aborted_tx_length);
    }

    usart_init(USART4,
               config->baudrate,
               config->data_bits,
               config->stop_bits);
    usart_parity_selection_config(USART4, config->parity);
    usart_transmitter_enable(USART4, TRUE);
    usart_receiver_enable(USART4, TRUE);
    usart_dma_transmitter_enable(USART4, TRUE);
    usart_dma_receiver_enable(USART4, TRUE);

    /* Start RX DMA. */
    dma_channel_enable(DMA1_CHANNEL1, FALSE);
    dma_flag_clear(DMA1_FDT1_FLAG);

    wk_dma_channel_config(DMA1_CHANNEL1,
                          (uint32_t)&USART4->dt,
                          (uint32_t)uart_rx_dma_buffer,
                          UART_RX_DMA_BUFFER_SIZE);

    uart_rx_last_pos = 0U;
    dma_channel_enable(DMA1_CHANNEL1, TRUE);

    usart_enable(USART4, TRUE);
    uart_configured = true;
}

void chry_dap_usb2uart_uart_config_callback(struct cdc_line_coding *line_coding)
{
    /**
     * Stop uart tx and rx immediately, discard all bytes in the tx and rx buffer,
     * reconfigure the uart with new line coding, then restart the uart tx and rx.
     */
    usb2uart_line_config_t config;

    if (uart_decode_line_config(line_coding, &config) == false) {
        return;
    }

    uart_apply_line_config(&config);
}


/* -------------------------------------------------------------------------- */
/*                                   CDC RX                                   */
/* -------------------------------------------------------------------------- */
void usb2uart_rx_poll(void)
{
    /**
    * CDC uart rx use circular dma, the main loop poll the dma write position
    * to copy new bytes into CherryDAP's g_uartrx.
    */
    uint32_t previous_pos;
    uint32_t current_pos;

    if (uart_configured == false) {
        return;
    }

    previous_pos = uart_rx_last_pos;
    current_pos = UART_RX_DMA_BUFFER_SIZE - dma_data_number_get(DMA1_CHANNEL1);

    if (current_pos == previous_pos) {
        return;
    }

    /* bytes are dropped when the CherryDAP's ring buffer is full */
    if (current_pos > previous_pos) {
        chry_ringbuffer_write(&g_uartrx,
                              (void *)&uart_rx_dma_buffer[previous_pos],
                              current_pos - previous_pos);
    } else {
        chry_ringbuffer_write(&g_uartrx,
                              (void *)&uart_rx_dma_buffer[previous_pos],
                              UART_RX_DMA_BUFFER_SIZE - previous_pos);
        chry_ringbuffer_write(&g_uartrx, (void *)uart_rx_dma_buffer, current_pos);
    }

    uart_rx_last_pos = current_pos;
}

/* -------------------------------------------------------------------------- */
/*                                   CDC TX                                   */
/* -------------------------------------------------------------------------- */
void chry_dap_usb2uart_uart_send_bydma(uint8_t *data, uint16_t len)
{
    if ((data == NULL) || (len == 0U)) {
        return;
    }

    if (uart_tx_active_length != 0U) {
        return;
    }

    dma_channel_enable(DMA1_CHANNEL2, FALSE);
    dma_flag_clear(DMA1_FDT2_FLAG);

    wk_dma_channel_config(DMA1_CHANNEL2,
                          (uint32_t)&USART4->dt,
                          (uint32_t)data,
                          len);
    uart_tx_active_length = len;
    dma_channel_enable(DMA1_CHANNEL2, TRUE);
}

void usb2uart_tx_dma_irq_handler(void)
{
    if (dma_flag_get(DMA1_FDT2_FLAG) != RESET) {
        uint32_t completed_length = uart_tx_active_length;

        dma_flag_clear(DMA1_FDT2_FLAG);
        dma_channel_enable(DMA1_CHANNEL2, FALSE);
        uart_tx_active_length = 0U;

        /* CherryDAP only advances g_usbrx and marks UART TX idle here. */
        if (completed_length != 0U) {
            chry_dap_usb2uart_uart_send_complete(completed_length);
        }
    }
}
