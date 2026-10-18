#include "Driver_USART.h"

#include "at32f425_wk_config.h"

typedef struct {
    ARM_USART_SignalEvent_t callback;
    volatile uint8_t initialized;
    volatile uint8_t powered;
    volatile uint8_t rx_busy;
    volatile uint32_t rx_length;
    volatile uint32_t rx_count;
} swo_usart_state_t;

static swo_usart_state_t swo_usart;

static void swo_usart_rx_abort(void)
{
    uint32_t remaining;

    dma_channel_enable(DMA1_CHANNEL3, FALSE);
    dma_flag_clear(DMA1_FDT3_FLAG | DMA1_DTERR3_FLAG);
    usart_dma_receiver_enable(USART2, FALSE);

    if (swo_usart.rx_busy != 0U) {
        remaining = dma_data_number_get(DMA1_CHANNEL3);
        swo_usart.rx_count = (remaining <= swo_usart.rx_length)
                                 ? (swo_usart.rx_length - remaining)
                                 : 0U;
        swo_usart.rx_busy = 0U;
    }
}

static ARM_DRIVER_VERSION swo_usart_get_version(void)
{
    ARM_DRIVER_VERSION version = {
        .api = ARM_DRIVER_VERSION_MAJOR_MINOR(2U, 4U),
        .drv = ARM_DRIVER_VERSION_MAJOR_MINOR(1U, 0U)
    };
    return version;
}

static ARM_USART_CAPABILITIES swo_usart_get_capabilities(void)
{
    ARM_USART_CAPABILITIES capabilities = { 0 };
    capabilities.asynchronous = 1U;
    return capabilities;
}

static int32_t swo_usart_initialize(ARM_USART_SignalEvent_t callback)
{
    swo_usart.callback = callback;
    swo_usart.rx_busy = 0U;
    swo_usart.rx_length = 0U;
    swo_usart.rx_count = 0U;
    swo_usart.initialized = 1U;
    return ARM_DRIVER_OK;
}

static int32_t swo_usart_uninitialize(void)
{
    swo_usart_rx_abort();
    usart_receiver_enable(USART2, FALSE);
    usart_enable(USART2, FALSE);
    swo_usart.powered = 0U;
    swo_usart.initialized = 0U;
    swo_usart.callback = 0;
    return ARM_DRIVER_OK;
}

static int32_t swo_usart_power_control(ARM_POWER_STATE state)
{
    if (swo_usart.initialized == 0U) {
        return ARM_DRIVER_ERROR;
    }

    if (state == ARM_POWER_FULL) {
        usart_enable(USART2, TRUE);
        swo_usart.powered = 1U;
        return ARM_DRIVER_OK;
    }

    if (state == ARM_POWER_OFF) {
        usart_receiver_enable(USART2, FALSE);
        swo_usart_rx_abort();
        usart_enable(USART2, FALSE);
        swo_usart.powered = 0U;
        return ARM_DRIVER_OK;
    }

    return ARM_DRIVER_ERROR_UNSUPPORTED;
}

static int32_t swo_usart_receive(void *data, uint32_t num)
{
    if ((swo_usart.powered == 0U) || (data == 0) || (num == 0U) || (num > 0xFFFFU)) {
        return ARM_DRIVER_ERROR_PARAMETER;
    }
    if (swo_usart.rx_busy != 0U) {
        return ARM_DRIVER_ERROR_BUSY;
    }

    dma_channel_enable(DMA1_CHANNEL3, FALSE);
    dma_flag_clear(DMA1_FDT3_FLAG | DMA1_DTERR3_FLAG);
    wk_dma_channel_config(DMA1_CHANNEL3,
                          (uint32_t)&USART2->dt,
                          (uint32_t)data,
                          (uint16_t)num);

    swo_usart.rx_length = num;
    swo_usart.rx_count = 0U;
    swo_usart.rx_busy = 1U;

    dma_interrupt_enable(DMA1_CHANNEL3, DMA_FDT_INT | DMA_DTERR_INT, TRUE);
    dma_channel_enable(DMA1_CHANNEL3, TRUE);
    return ARM_DRIVER_OK;
}

static uint32_t swo_usart_get_rx_count(void)
{
    if (swo_usart.rx_busy != 0U) {
        uint32_t remaining = dma_data_number_get(DMA1_CHANNEL3);
        return (remaining <= swo_usart.rx_length)
                   ? (swo_usart.rx_length - remaining)
                   : 0U;
    }
    return swo_usart.rx_count;
}

static int32_t swo_usart_control(uint32_t control, uint32_t arg)
{
    if ((swo_usart.powered == 0U) &&
        (control != ARM_USART_CONTROL_RX) &&
        (control != ARM_USART_ABORT_RECEIVE)) {
        return ARM_DRIVER_ERROR;
    }

    if (control == ARM_USART_CONTROL_RX) {
        if (arg != 0U) {
            if (swo_usart.rx_busy == 0U) {
                return ARM_DRIVER_ERROR;
            }
            usart_enable(USART2, TRUE);
            usart_dma_receiver_enable(USART2, TRUE);
            usart_receiver_enable(USART2, TRUE);
        } else {
            usart_receiver_enable(USART2, FALSE);
            usart_dma_receiver_enable(USART2, FALSE);
        }
        return ARM_DRIVER_OK;
    }

    if (control == ARM_USART_ABORT_RECEIVE) {
        swo_usart_rx_abort();
        return ARM_DRIVER_OK;
    }

    if ((control & ARM_USART_CONTROL_Msk) == ARM_USART_MODE_ASYNCHRONOUS) {
        if ((control & ARM_USART_DATA_BITS_Msk) != ARM_USART_DATA_BITS_8 ||
            (control & ARM_USART_PARITY_Msk) != ARM_USART_PARITY_NONE ||
            (control & ARM_USART_STOP_BITS_Msk) != ARM_USART_STOP_BITS_1 ||
            (control & ARM_USART_FLOW_CONTROL_Msk) != ARM_USART_FLOW_CONTROL_NONE ||
            arg == 0U) {
            return ARM_DRIVER_ERROR_PARAMETER;
        }

        usart_enable(USART2, FALSE);
        usart_receiver_enable(USART2, FALSE);
        usart_dma_receiver_enable(USART2, FALSE);
        usart_init(USART2, arg, USART_DATA_8BITS, USART_STOP_1_BIT);
        usart_parity_selection_config(USART2, USART_PARITY_NONE);
        usart_hardware_flow_control_set(USART2, USART_HARDWARE_FLOW_NONE);
        usart_enable(USART2, TRUE);
        return ARM_DRIVER_OK;
    }

    return ARM_DRIVER_ERROR_UNSUPPORTED;
}

static ARM_USART_STATUS swo_usart_get_status(void)
{
    ARM_USART_STATUS status = { 0 };
    status.rx_busy = swo_usart.rx_busy;
    return status;
}

static uint32_t swo_usart_get_tx_count(void)
{
    return 0U;
}

static int32_t swo_usart_unsupported_send(const void *data, uint32_t num)
{
    (void)data;
    (void)num;
    return ARM_DRIVER_ERROR_UNSUPPORTED;
}

static int32_t swo_usart_unsupported_transfer(const void *data_out,
                                               void *data_in,
                                               uint32_t num)
{
    (void)data_out;
    (void)data_in;
    (void)num;
    return ARM_DRIVER_ERROR_UNSUPPORTED;
}

static int32_t swo_usart_unsupported_modem_control(ARM_USART_MODEM_CONTROL control)
{
    (void)control;
    return ARM_DRIVER_ERROR_UNSUPPORTED;
}

static ARM_USART_MODEM_STATUS swo_usart_unsupported_modem_status(void)
{
    ARM_USART_MODEM_STATUS status = { 0 };
    return status;
}

ARM_DRIVER_USART Driver_USART0 = {
    .GetCapabilities = swo_usart_get_capabilities,
    .GetVersion = swo_usart_get_version,
    .Initialize = swo_usart_initialize,
    .Uninitialize = swo_usart_uninitialize,
    .PowerControl = swo_usart_power_control,
    .Send = swo_usart_unsupported_send,
    .Receive = swo_usart_receive,
    .Transfer = swo_usart_unsupported_transfer,
    .GetTxCount = swo_usart_get_tx_count,
    .GetRxCount = swo_usart_get_rx_count,
    .Control = swo_usart_control,
    .GetStatus = swo_usart_get_status,
    .SetModemControl = swo_usart_unsupported_modem_control,
    .GetModemStatus = swo_usart_unsupported_modem_status
};

void swo_rx_dma_irq_handler(void)
{
    if (dma_flag_get(DMA1_FDT3_FLAG) == SET) {
        dma_channel_enable(DMA1_CHANNEL3, FALSE);
        dma_flag_clear(DMA1_FDT3_FLAG);
        swo_usart.rx_count = swo_usart.rx_length;
        swo_usart.rx_busy = 0U;

        if (swo_usart.callback != 0) {
            swo_usart.callback(ARM_USART_EVENT_RECEIVE_COMPLETE);
        }
    } else if (dma_flag_get(DMA1_DTERR3_FLAG) == SET) {
        dma_channel_enable(DMA1_CHANNEL3, FALSE);
        dma_flag_clear(DMA1_DTERR3_FLAG);
        swo_usart.rx_busy = 0U;

        if (swo_usart.callback != 0) {
            swo_usart.callback(ARM_USART_EVENT_RX_OVERFLOW);
        }
    }
}
