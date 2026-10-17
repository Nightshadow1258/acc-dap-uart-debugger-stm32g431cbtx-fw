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
#include <zephyr/drivers/gpio.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/class/usb_hid.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

static const struct device *usb_cdc_dev = DEVICE_DT_GET(DT_NODELABEL(cdc_acm_uart0));
static const struct device *hw_uart_dev = DEVICE_DT_GET(DT_NODELABEL(usart1));
static const struct device *hid_dev;

#define RING_BUF_SIZE 1024
RING_BUF_DECLARE(cdc_to_hw_ring, RING_BUF_SIZE);
RING_BUF_DECLARE(hw_to_cdc_ring, RING_BUF_SIZE);

/* CMSIS-DAP v1 HID Report Descriptor */
static const uint8_t hid_report_desc[] = {
    0x06, 0x00, 0xFF,  /* Usage Page (Vendor Defined 0xFF00) */
    0x09, 0x01,        /* Usage (Vendor Usage 1) */
    0xA1, 0x01,        /* Collection (Application) */
    0x15, 0x00,        /* Logical Minimum (0) */
    0x26, 0xFF, 0x00,  /* Logical Maximum (255) */
    0x75, 0x08,        /* Report Size (8 bits) */
    0x95, 0x40,        /* Report Count (64 bytes) */
    0x09, 0x02,        /* Usage (Vendor Usage 2) */
    0x81, 0x02,        /* Input (Data, Var, Abs) */
    0x09, 0x03,        /* Usage (Vendor Usage 3) */
    0x91, 0x02,        /* Output (Data, Var, Abs) */
    0xC0               /* End Collection */
};

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

    hid_dev = device_get_binding("HID_0");
    if (hid_dev == NULL) {
        LOG_ERR("Cannot get USB HID Device");
        return 0;
    }

    usb_hid_register_device(hid_dev, hid_report_desc, sizeof(hid_report_desc), NULL);
    usb_hid_init(hid_dev);

    ret = usb_enable(NULL);
    if (ret != 0) {
        LOG_ERR("Failed to enable USB stack (%d)", ret);
        return 0;
    }

    uart_irq_callback_set(usb_cdc_dev, cdc_acm_cb);
    uart_irq_callback_set(hw_uart_dev, hw_uart_cb);

    uart_irq_rx_enable(usb_cdc_dev);
    uart_irq_rx_enable(hw_uart_dev);

    LOG_INF("Composite USB Debugger active.");

    while (1) {
        k_msleep(1000);
    }

    return 0;
}