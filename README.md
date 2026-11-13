# CMSIS-DAP v2 & USB-UART Bridge Firmware (STM32G431CBTx)

Zephyr RTOS-based firmware turning a custom **STM32G431CBTx** board into a composite USB debug probe featuring a **CMSIS-DAP v2** (WinUSB/Bulk) SWD interface and a **CDC-ACM USB-to-UART bridge**.

* **Hardware Repository:** [acc-dap-uart-debugger-stm32g431cbtx-hw](https://github.com/Nightshadow1258/acc-dap-uart-debugger-stm32g431cbtx-hw)

---

## Features

* **CMSIS-DAP v2 (Bulk / WinUSB):** High-speed SWD debugging via Zephyr's `USB_DEVICE_STACK_NEXT` and bit-banged `zephyr,swdp-gpio` driver running at a 170 MHz system clock.
* **USB-to-UART Bridge (CDC-ACM):** Bi-directional serial passthrough (`USART2` on `PA2`/`PA3`) exposed as `/dev/ttyACM0` (Linux) or a COM port (Windows).
* **Crystal-less USB Clocking:** Uses the STM32G431CBTx internal 48 MHz RC oscillator (`HSI48`) for USB full-speed operation.
* **Dedicated Debug Console:** Routes Zephyr kernel/shell logs to `USART1` (`PA9`/`PA10`) so log messages never corrupt the CDC-ACM UART bridge stream.

---

## Hardware & Pinout

### Probe GPIO Mapping (Devicetree)

The SWD and UART signals are defined in the board devicetree as follows:

| Function | STM32G431 Pin | Devicetree Node / Property | Notes |
| :--- | :--- | :--- | :--- |
| **SWCLK** | `PA5` | `dp0` (`clk-gpios`) | Active High |
| **SWDIO** | `PA4` | `dp0` (`dio-gpios`) | Internal Pull-Up enabled |
| **nRESET** | `PB0` | `dp0` (`reset-gpios`) | Target reset control |
| **Bridge TX** | `PA2` | `usart2` (`usart2_tx_pa2`) | Connect to Target RX |
| **Bridge RX** | `PA3` | `usart2` (`usart2_rx_pa3`) | Connect to Target TX |
| **Debug Console TX** | `PA9` | `usart1` (`usart1_tx_pa9`) | Optional probe firmware logs |
| **Debug Console RX** | `PA10` | `usart1` (`usart1_rx_pa10`) | Optional probe firmware logs |
| **USB D- / D+** | `PA11` / `PA12` | `usb` (`usb_dm_pa11`, `usb_dp_pa12`) | Clocked via internal `HSI48` |

### TC2050 10-Pin Tag-Connect Pinout

The hardware connects to the target board using a 10-pin **TC2050** Tag-Connect cable with the following pinout:

| Pin Name | Pin Number | Pin Number | Pin Name |
| :--- | :---: | :---: | :--- |
| `VDD/XXX` | **1** | **2** | `GND` |
| `UART_RXD` | **3** | **4** | `UART_TXD` |
| `JTMS_SWDIO` | **5** | **6** | `JTCK_SWCLK` |
| `JTDO_SWO` | **7** | **8** | `JTDI` |
| `XXX` | **9** | **10** | `JTRST` |

> **Note:** Ensure `UART_RXD` (Pin 3) and `UART_TXD` (Pin 4) are crossed appropriately relative to the target MCU's TX and RX lines.

---

## Building & Flashing the Probe Firmware

Build and flash the firmware onto the STM32G431CBTx probe using Zephyr's `west` tool:

```bash
west build -b <your_custom_board_name> --pristine
west flash
```

---

## Usage

### 1. Integrating with Zephyr (`board.cmake`)

To use this probe as the default runner for a target Zephyr project (e.g., an STM32F4 target), add the following to your target board's `board.cmake`:

```cmake
board_runner_args(openocd --config interface/cmsis-dap.cfg --config target/stm32f4x.cfg)
```

*(Optional)* If debugging over long jumper wires or noisy lines, you can cap the SWD clock speed by adding `-c "adapter speed 500"`:

```cmake
board_runner_args(openocd --config interface/cmsis-dap.cfg -c "adapter speed 500" --config target/stm32f4x.cfg)
```

Once configured, flash and debug your target directly with `west`:

```bash
west flash
west debug
```

### 2. Standalone OpenOCD Usage

To start an OpenOCD GDB server manually from the command line:

```bash
openocd -f interface/cmsis-dap.cfg -f target/stm32f4x.cfg
```

### 3. Using the USB-to-UART Bridge

When plugged in, the probe enumerates a standard USB CDC-ACM serial port (e.g., `/dev/ttyACM0` on Linux). Open it with any serial terminal (`minicom`, `picocom`, `screen`, or PuTTY) at the target's baud rate (default `115200`):

```bash
picocom -b 115200 /dev/ttyACM0
```

### 4. Linux udev Permissions (Optional)

To run OpenOCD and access `/dev/ttyACM*` without `sudo`, add a udev rule at `/etc/udev/rules.d/60-cmsis-dap.rules`:

```udev
# CMSIS-DAP v2 Probe (VID: 0x2fe3, PID: 0x0204)
SUBSYSTEM=="usb", ATTR{idVendor}=="2fe3", ATTR{idProduct}=="0204", MODE="0666", GROUP="plugdev"
SUBSYSTEM=="tty", ATTRS{idVendor}=="2fe3", ATTRS{idProduct}=="0204", MODE="0666", GROUP="plugdev"
```

Reload udev rules to apply:

```bash
sudo udevadm control --reload-rules && sudo udevadm trigger
```