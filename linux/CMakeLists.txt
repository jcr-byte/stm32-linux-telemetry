cmake_minimum_required(VERSION 3.16)
project(telemetry LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Writes build/compile_commands.json so clangd knows the exact flags.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

add_executable(telemetry
  src/main.c
  src/serial.c
)

target_compile_options(telemetry PRIVATE -Wall -Wextra -Wpedantic)
