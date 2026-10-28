# STM32 + Linux Embedded Sensor Telemetry System

**STM32 firmware that reads a BME280 sensor and streams measurements to a Linux telemetry service**

## Project overview

Build a sensor telemetry system with two independently programmed components:

1. **STM32 microcontroller firmware**, written in C using STM32CubeIDE (or a GCC/CMake-based STM32 project) and STM32 HAL/LL. It measures temperature, relative humidity, and atmospheric pressure from a BME280 sensor over I²C and sends readings to the host over UART.
2. **Linux telemetry service**, written in C, running on a Linux desktop, laptop, or single-board computer. It receives serial measurements, validates and timestamps them, stores them, handles disconnections, and runs under `systemd`.

The **NUCLEO-F446RE** includes an onboard ST-LINK debugger and virtual COM port (VCP). The ST-LINK USB connection bridges the STM32 UART to a serial device on Linux; **no STM32 USB CDC firmware is necessary**. A separate 3.3 V USB-to-UART adapter is an alternative if the board’s VCP connection is unavailable.

**Primary languages:** C and shell.  
**Host OS:** Ubuntu Linux (native or VM with USB passthrough).

**Hardware platform:** **STMicroelectronics NUCLEO-F446RE** (STM32F446RE MCU, onboard ST-LINK debugger and virtual COM port). The pin assignments below target this board. Confirm solder-bridge routing and your breakout board’s connections before wiring.

## System architecture

```text
 BME280 sensor              STM32 Nucleo board                     Linux host
 (temp/humidity/pressure)   (STM32 C firmware)                     (C service)
         |                         |                                     |
         +------ I2C ------------>+                                     |
                                   +-- UART TX --> ST-LINK VCP bridge --+
                                                USB cable               |
                                                                        |
                                                                 Parse + validate
                                                                        |
                                                               Timestamp + persist
                                                                        |
                                                                 CSV or SQLite
                                                                        |
                                                                systemd + logs
```

**Firmware:** Configure clocks/GPIO/I²C/UART, read and compensate measurements, format messages, transmit, report errors, and optionally use a watchdog.  
**Linux application:** Find/open the serial port, buffer partial reads, decode/validate, recover from disconnects, persist measurements, and produce status/logs.

## Hardware connections

### BME280 wiring — NUCLEO-F446RE

Connect the BME280 to **I²C1** on the NUCLEO-F446RE using **PB8 (SCL)** and **PB9 (SDA)**. Configure both pins for the I²C alternate function in STM32CubeIDE. Confirm the header positions against the board pinout and verify sensor-breakout voltage requirements before applying power.

| BME280 pin | Connect to STM32 |
|---|---|
| VCC/VIN | **3V3**, if supported by the breakout |
| GND | **GND** |
| SDA | **PB9 (I²C1_SDA)** |
| SCL | **PB8 (I²C1_SCL)** |

I²C requires pull-ups on SDA/SCL; many breakout boards include them. If the breakout exposes **CS/SDO**, follow its datasheet to select I²C mode and an address (typically `0x76` or `0x77`). **Do not apply 5 V to unverified STM32 GPIO pins.**

### UART and ST-LINK virtual COM port

Use **USART2** on **PA2 (TX)** and **PA3 (RX)** for the NUCLEO-F446RE ST-LINK virtual COM port, subject to the board’s solder-bridge configuration. Configure **115200 baud, 8 data bits, no parity, 1 stop bit (8N1)**.

Connect the Nucleo board's ST-LINK USB port to Linux. The device will generally appear as `/dev/ttyACM*` or another `/dev/tty*` path, depending on the bridge/interface. USB serial-device numbering can change. If using an external adapter, cross-connect **STM32 TX → adapter RX**, and share GND; connect RX only if bidirectional communication is needed. Use a 3.3 V adapter.

## Development prerequisites

### Host-side Linux tools

```bash
sudo apt update
sudo apt install git build-essential cmake gdb minicom
```

### STM32 tools

- **STM32CubeIDE** for project setup, HAL generation, firmware build and source-level debugging.
- **STM32CubeMX** pin/clock/peripheral configuration (integrated with CubeIDE where supported).
- **ST-LINK / SWD** to flash and debug firmware; usually integrated on Nucleo boards.
- Optional: **STM32CubeProgrammer**, an Arm GCC toolchain, and OpenOCD for alternative flashing/debugging workflows.


## Implementation stages

### Firmware project and timing base

1. Create a STM32CubeIDE project targeting the **NUCLEO-F446RE (STM32F446RE)** and configure the MCU clocks.
2. Implement a nonblocking main-loop scheduler using the system tick (`HAL_GetTick()`) or a hardware timer, so periodic tasks run at a configurable rate without busy-wait delays.
3. Drive LED2 as a heartbeat/status indicator from the scheduler (e.g. steady blink when healthy, fast blink on sensor error).

**Done when:** The firmware flashes over ST-LINK and runs periodic tasks at a configurable rate without blocking.

### BME280 measurement over I²C

1. Verify the breakout's 3.3 V requirements, I²C address, pull-ups, and CS/SDO mode.
2. Configure **I²C1 on PB8/PB9** in CubeMX.
3. Read the BME280 chip ID (`0x60`) and validate that communication works.
4. Read sensor registers and apply Bosch's BME280 compensation algorithm or a well-understood sensor driver.
5. Report realistic temperature (°C), humidity (%RH), and pressure (hPa) values and explicit sensor failures.

**Done when:** Valid measurements are produced repeatedly and errors are distinguishable from data.

### STM32 UART telemetry into Linux

1. Configure **USART2 on PA2/PA3** at 115200 8N1 through the onboard ST-LINK VCP (or use an external 3.3 V USB-UART adapter if needed).
2. Send newline-terminated records such as:

   ```text
   TEMP=23.5,HUM=42.1,PRESS=1012.8
   ```

3. On Linux, locate the port using `dmesg`, `ls /dev/ttyACM*` (if applicable), and `/dev/serial/by-id/` (if available).
4. Verify the stream using `minicom -D /dev/ttyACM0 -b 115200`, replacing the port path as necessary.
5. Implement a Linux C reader with `open()`, `termios`, `read()`, and line buffering.

**Done when:** The Linux program correctly parses messages even when lines are split over multiple read calls.

### Linux service and persistence

1. Timestamp and store valid measurements in CSV (or SQLite).
2. Add error reporting and structured diagnostic logs.
3. Support graceful `SIGTERM`/`SIGINT` shutdown.
4. Handle physical device unplugging, serial read errors, and reconnects; prefer a stable device path where available.
5. Create a `systemd` unit, configure appropriate serial permissions, and inspect logs with `journalctl`.
6. Test restarting the service and rebooting the Linux host.

**Done when:** The service restarts and recovers from expected serial-port interruptions without corrupting data.

### Reliability and protocol testing

1. Introduce a protocol version, sequence counter, and explicit sensor-error status.
2. Add framing, a maximum length, and checksum/CRC to detect corruption.
3. Test malformed, partial, duplicate, and out-of-order messages.
4. Unit-test protocol parsing on Linux; compile pure protocol routines outside STM32CubeIDE when practical.
5. Add a watchdog with clear reset criteria if appropriate.
6. Document setup, board-specific wiring, troubleshooting, test procedure, and limitations.

**Done when:** Automated parser tests pass and the whole system is reproducible from your README.

## Suggested repository structure

```text
stm32-linux-telemetry/
├── firmware/
│   ├── Core/                  # STM32CubeIDE/CubeMX-generated startup and app code
│   ├── Drivers/               # STM32 HAL/LL and CMSIS
│   ├── sensor/                # BME280 driver/compensation
│   ├── protocol/              # Telemetry encoding
│   └── README.md              # Exact board/pin/clock/UART setup
├── linux/
│   ├── CMakeLists.txt
│   ├── src/
│   │   ├── main.c
│   │   ├── serial.c
│   │   ├── parser.c
│   │   └── logger.c
│   └── systemd/
│       └── telemetry.service
├── tests/
│   ├── test_protocol.c
│   └── test_parser.c
└── README.md
```

## Functional requirements

1. Sample the BME280 at a configurable rate (start at once per second).
2. Send a newline-delimited record over STM32 UART through ST-LINK VCP or a 3.3 V USB-UART adapter.
3. Validate each record; reject malformed, impossible, incomplete, and overlong input.
4. Define units and persist host-side timestamps; optionally include MCU uptime.
5. Avoid unbounded buffering and handle partial/combined Linux serial reads.
6. Recover from unplug/replug or MCU reset without manual intervention.
7. Shut down gracefully and log meaningful errors.
8. Keep protocol parsing unit-testable on Linux.

## Stretch goals

- Replace polling UART writes with interrupt- or DMA-driven transfers.
- Use I²C interrupts or DMA and investigate concurrency and bus fault recovery.
- Add a real-time operating system to the STM32 (for example, FreeRTOS).
- Implement native USB CDC on a suitable STM32 and compare it with ST-LINK UART VCP.
- Add a simple local dashboard or network endpoint on Linux.
- Move the Linux service to a Raspberry Pi or another embedded Linux board.
- Build a minimal Linux image with Buildroot.

