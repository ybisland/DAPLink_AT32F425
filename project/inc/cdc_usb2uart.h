#ifndef CDC_USB2UART_H
#define CDC_USB2UART_H

/* Call after usart and dma are initialized. */
void usb2uart_init(void);

/* Call from the main loop to move UART RX DMA data into CherryDAP. */
void usb2uart_rx_poll(void);

#endif /* CDC_USB2UART_H */