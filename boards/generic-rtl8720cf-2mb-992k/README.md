# Generic - RTL8720CF (2M/992k)

*by Generic*

[Product page](https://kuba2k2.github.io/libretuya/boards/generic-rtl8720cf-2mb-992k/)

- [General info](../../docs/platform/realtek-amb/README.md)

Parameter    | Value
-------------|-----------------------------
Board code   | `generic-rtl8720cf-2mb-992k`
MCU          | RTL8720CF
Manufacturer | Realtek
Series       | AmebaZ2
Frequency    | 100 MHz
Flash size   | 2 MiB
RAM size     | 256 KiB
Voltage      | 3.0V - 3.6V
I/O          | 20x GPIO, 8x PWM, 3x UART
Wi-Fi        | 802.11 b/g/n
BLE          | v4.2

## Usage

**Board code:** `generic-rtl8720cf-2mb-992k`

In `platformio.ini`:

```ini
[env:generic-rtl8720cf-2mb-992k]
platform = libretuya
board = generic-rtl8720cf-2mb-992k
framework = arduino
```

In ESPHome YAML:

```yaml
libretuya:
  board: generic-rtl8720cf-2mb-992k
  framework:
    version: dev
```

## Arduino Core pin mapping

No. | Pin  | UART      | I²C      | SPI       | PWM  | Other
----|------|-----------|----------|-----------|------|-----------
D0  | PA00 | UART1_RX  |          |           | PWM0 | SWCLK, TCK
D1  | PA01 | UART1_TX  |          |           | PWM1 | SWDIO, TMS
D2  | PA02 | UART1_RX  | I2C0_SCL | SPI0_CS   | PWM2 | TDO
D3  | PA03 | UART1_TX  | I2C0_SDA | SPI0_SCK  | PWM3 | TDI
D4  | PA04 | UART1_CTS |          | SPI0_MOSI | PWM4 | tRST
D5  | PA07 |           |          | SPI0_CS   |      |
D6  | PA08 |           |          | SPI0_SCK  |      |
D7  | PA09 | UART0_RTS |          | SPI0_MOSI |      |
D8  | PA10 | UART0_CTS |          | SPI0_MISO |      |
D9  | PA11 | UART0_TX  | I2C0_SCL |           | PWM0 |
D10 | PA12 | UART0_RX  | I2C0_SDA |           | PWM1 |
D11 | PA13 | UART0_RX  |          |           | PWM7 |
D12 | PA14 | UART0_TX  |          |           | PWM2 |
D13 | PA15 | UART2_RX  | I2C0_SCL | SPI0_CS   | PWM3 |
D14 | PA16 | UART2_TX  | I2C0_SDA | SPI0_SCK  | PWM4 |
D15 | PA17 |           |          |           | PWM5 |
D16 | PA18 |           |          |           | PWM6 |
D17 | PA19 | UART2_CTS | I2C0_SCL | SPI0_MOSI | PWM7 |
D18 | PA20 | UART2_RTS | I2C0_SDA | SPI0_MISO | PWM0 |
D19 | PA23 |           |          |           | PWM7 |

## Flash memory map

Flash size: 2 MiB / 2,097,152 B / 0x200000

Hex values are in bytes.

Name            | Start    | Length            | End
----------------|----------|-------------------|---------
Partition Table | 0x000000 | 4 KiB / 0x1000    | 0x001000
System Data     | 0x001000 | 4 KiB / 0x1000    | 0x002000
Calibration     | 0x002000 | 4 KiB / 0x1000    | 0x003000
(reserved)      | 0x003000 | 4 KiB / 0x1000    | 0x004000
Boot Image      | 0x004000 | 32 KiB / 0x8000   | 0x00C000
OTA1 Image      | 0x00C000 | 992 KiB / 0xF8000 | 0x104000
OTA2 Image      | 0x104000 | 992 KiB / 0xF8000 | 0x1FC000
Key-Value Store | 0x1FC000 | 8 KiB / 0x2000    | 0x1FE000
User Data       | 0x1FE000 | 8 KiB / 0x2000    | 0x200000
