# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

STM32 firmware (NUCLEO-F446RE) reads a BME280 over I²C and streams readings over UART (ST-LINK virtual COM port) to a Linux telemetry service written in C. `docs/embedded_telemetry_project.md` is the full spec: staged plan, wiring, wire-protocol goals, functional requirements and the target repo layout. Read it before starting a new stage.

Current state: only `firmware/` exists (stages 1–2 done, UART output is human-readable debug text). The `linux/`, `tests/` and `firmware/protocol/` directories in the spec haven't been created yet.

## Working with me

I'm a student and brand new to systems programming, so act as a tutor:

- Work in small chunks: one function or one concept per change. After each chunk, stop so I can review it, ask questions, and test it before we move on.
- Explain the important concepts each chunk introduces (e.g. file descriptors, `termios`, `poll()`, `errno`): what the concept is and why this code needs it, tied to the specific lines that use it.

## Building and flashing

- The firmware is an **STM32CubeIDE managed-build project** (`firmware/.cproject`, `firmware/firmware.ioc`). There is no Makefile or CMake: build, flash and debug happen in CubeIDE (`firmware Debug.launch` is the ST-LINK debug config). The CLI has no ARM toolchain on PATH, so firmware changes can't be compiled from here. Say so rather than claiming a build passed.
- Build output (`firmware/Debug/`, `firmware/Release/`) is gitignored.
- `firmware/compile_flags.txt` exists only for clangd. It lacks `-IApp -Isensor` and points `-isystem` at a macOS CubeIDE install, so clangd errors on this Linux machine don't mean the code is wrong.
- To view output: `minicom -D /dev/ttyACM0 -b 115200` (115200 8N1; the port number can change, so check `/dev/serial/by-id/`).

## Firmware architecture

- **CubeMX-generated code** (`Core/`, `Drivers/`, `startup_*.s`, linker scripts): regenerating from `firmware.ioc` overwrites everything outside `/* USER CODE BEGIN … */ … /* USER CODE END … */` blocks. Keep edits to `Core/` inside those blocks, and put real logic elsewhere.
- **`App/`**: the application layer. `main.c` calls `app_init(&hi2c1, &huart2)` once and `app_tick(HAL_GetTick())` every loop. `app_tick` is a nonblocking scheduler: each task keeps a `last_tick` and advances it by its fixed period (`now - last >= PERIOD`, which is wraparound-safe). Don't add `HAL_Delay` to the loop.
- **`sensor/`** is split in two:
  - `bme280.c`: the HAL-dependent I²C driver (address probe 0x76/0x77, chip-ID check, reset, calibration read, register config).
  - `bme280_comp.c/.h`: pure, `stdint.h`-only parsing and Bosch integer compensation. It has no HAL dependency, so it can be compiled and unit-tested on Linux. Keep it that way.
- New source folders must be added to CubeIDE's source entries and include paths in `.cproject`, or they won't build.

## Conventions and gotchas

- Measurements are fixed-point integers: `temp_centi_c` (°C×100), `press_pa` (Pa), `hum_milli_pct` (%RH×1000). Format them with integer `snprintf`, because newlib-nano printf has no `%f`.
- The sensor is a genuine BME280 (chip ID 0x60), so humidity must be supported. The BMP280 (0x58) branch is diagnostic only.
- Error recovery is deliberate. On a read failure `sensor_ok` is cleared and init is retried every sample period. `sensor_check()` clears a stuck I²C BUSY flag by re-running `HAL_I2C_Init` and prints bus-level diagnostics. The first period after a successful init skips the read because no measurement is ready yet.
- Pins: I²C1 on PB8 (SCL)/PB9 (SDA) at 100 kHz, USART2 on PA2/PA3, LED2 as the heartbeat via the Nucleo BSP.
