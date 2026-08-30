/*
 * Copyright (c) 2025
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file 
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

static const struct device *usb_cdc_dev = DEVICE_DT_GET(DT_NODELABEL(cdc_acm_uart0));
static const struct device *hw_uart_dev = DEVICE_DT_GET(DT_NODELABEL(usart1));

#define RING_BUF_SIZE 1024

RING_BUF_DECLARE(cdc_to_hw_ring, RING_BUF_SIZE);
RING_BUF_DECLARE(hw_to_cdc_ring, RING_BUF_SIZE);

/* Interrupt handler for USB CDC ACM */
static void cdc_acm_cb(const struct device *dev, void *user_data)
{
    uint8_t buf[64];
    int read_len;

    while (uart_irq_update(dev) && uart_irq_is_pending(dev)) {
        if (uart_irq_rx_ready(dev)) {
            read_len = uart_fifo_read(dev, buf, sizeof(buf));
            if (read_len > 0) {
                ring_buf_put(&cdc_to_hw_ring, buf, read_len);
                uart_irq_tx_enable(hw_uart_dev);
            }
        }

        if (uart_irq_tx_ready(dev)) {
            uint8_t tx_buf[64];
            uint32_t bytes_to_send = ring_buf_get(&hw_to_cdc_ring, tx_buf, sizeof(tx_buf));

            if (bytes_to_send == 0) {
                uart_irq_tx_disable(dev);
            } else {
                uart_fifo_fill(dev, tx_buf, bytes_to_send);
            }
        }
    }
}

/* Interrupt handler for Hardware USART1 */
static void hw_uart_cb(const struct device *dev, void *user_data)
{
    uint8_t buf[64];
    int read_len;

    while (uart_irq_update(dev) && uart_irq_is_pending(dev)) {
        if (uart_irq_rx_ready(dev)) {
            read_len = uart_fifo_read(dev, buf, sizeof(buf));
            if (read_len > 0) {
                ring_buf_put(&hw_to_cdc_ring, buf, read_len);
                uart_irq_tx_enable(usb_cdc_dev);
            }
        }

        if (uart_irq_tx_ready(dev)) {
            uint8_t tx_buf[64];
            uint32_t bytes_to_send = ring_buf_get(&cdc_to_hw_ring, tx_buf, sizeof(tx_buf));

            if (bytes_to_send == 0) {
                uart_irq_tx_disable(dev);
            } else {
                uart_fifo_fill(dev, tx_buf, bytes_to_send);
            }
        }
    }
}

int main(void)
{
    int ret;

    if (!device_is_ready(usb_cdc_dev) || !device_is_ready(hw_uart_dev)) {
        LOG_ERR("UART or USB CDC devices not ready");
        return 0;
    }

    ret = usb_enable(NULL);
    if (ret != 0) {
        LOG_ERR("Failed to enable USB stack (%d)", ret);
        return 0;
    }

    /* Configure UART Interrupt Callbacks */
    uart_irq_callback_set(usb_cdc_dev, cdc_acm_cb);
    uart_irq_callback_set(hw_uart_dev, hw_uart_cb);

    /* Enable RX interrupts on both channels */
    uart_irq_rx_enable(usb_cdc_dev);
    uart_irq_rx_enable(hw_uart_dev);

    LOG_INF("Interrupt driven USB-UART bridge active.");

    while (1) {
        k_sleep(K_FOREVER);
    }

    return 0;
}