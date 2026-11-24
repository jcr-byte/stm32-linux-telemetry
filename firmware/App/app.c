#include "app.h"
#include <stdio.h>
#include "stm32f4xx_nucleo.h"
#include "bme280.h"

#define LED_PERIOD_MS    500u
#define SAMPLE_PERIOD_MS 1000u

#define BME280_REG_CHIP_ID 0xD0u
#define BME280_CHIP_ID     0x60u
#define BMP280_CHIP_ID     0x58u

static I2C_HandleTypeDef *app_i2c;
static UART_HandleTypeDef *app_uart;

static uint32_t led_last_tick;
static uint32_t sample_last_tick;

static bme280_t sensor;
static int sensor_ok;

static void sensor_check(void)
{
  char msg[80];
  int n;
  uint8_t addr_found = 0;
  int bus_reset = 0;

  /* A glitch on SCL/SDA can leave the peripheral's BUSY flag stuck, which makes
     every later transfer time out. HAL_I2C_Init() software-resets the peripheral. */
  if (__HAL_I2C_GET_FLAG(app_i2c, I2C_FLAG_BUSY)) {
    HAL_I2C_Init(app_i2c);
    bus_reset = 1;
  }

  for (uint8_t addr = 0x76; addr <= 0x77; addr++) {
    if (HAL_I2C_IsDeviceReady(app_i2c, addr << 1, 3, 10) == HAL_OK) {
      addr_found = addr;
      break;
    }
  }

  if (addr_found == 0) {
    /* Idle I2C lines should read high (1) through the pull-ups. */
    int scl = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_8) == GPIO_PIN_SET;
    int sda = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_9) == GPIO_PIN_SET;
    int others = 0;

    for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
      if (HAL_I2C_IsDeviceReady(app_i2c, addr << 1, 1, 2) == HAL_OK) {
        others = addr;
        break;
      }
    }

    n = snprintf(msg, sizeof msg, "No BME280. SCL=%d SDA=%d err=0x%lX other=0x%02X bus_reset=%d\r\n",
                 scl, sda, (unsigned long)app_i2c->ErrorCode, others, bus_reset);
  } else {
    uint8_t id = 0;
    if (HAL_I2C_Mem_Read(app_i2c, addr_found << 1, BME280_REG_CHIP_ID,
                         I2C_MEMADD_SIZE_8BIT, &id, 1, 100) != HAL_OK) {
      n = snprintf(msg, sizeof msg, "Found 0x%02X but chip ID read failed\r\n", addr_found);
    } else {
      n = snprintf(msg, sizeof msg, "Found 0x%02X, chip ID 0x%02X (%s)\r\n", addr_found, id,
                   id == BME280_CHIP_ID ? "BME280" :
                   id == BMP280_CHIP_ID ? "BMP280 - no humidity" : "unknown");
    }
  }

  HAL_UART_Transmit(app_uart, (uint8_t *)msg, (uint16_t)n, 100);
}

/* Integer formatting: newlib-nano's printf has no %f by default. */
static void sensor_report(void)
{
  char msg[80];
  int n;
  bme280_meas_t m;
  bme280_status_t st = bme280_read(&sensor, &m);

  if (st != BME280_OK) {
    n = snprintf(msg, sizeof msg, "BME280 read failed (%d)\r\n", (int)st);
    sensor_ok = 0;   /* re-init next period; a power glitch also loses the config */
  } else {
    n = snprintf(msg, sizeof msg, "TEMP=%ld,HUM=%lu,PRESS=%lu\r\n",
                 (long)m.temp_centi_c, (unsigned long)m.hum_milli_pct,
                 (unsigned long)m.press_pa);
  }

  HAL_UART_Transmit(app_uart, (uint8_t *)msg, (uint16_t)n, 100);
}

void app_init(I2C_HandleTypeDef *hi2c, UART_HandleTypeDef *huart) 
{
  app_i2c = hi2c;
  app_uart = huart;

  sensor_ok = bme280_init(&sensor, app_i2c) == BME280_OK;

  led_last_tick = HAL_GetTick();
  sample_last_tick = led_last_tick;
}


void app_tick(uint32_t now)
{
  if (now - led_last_tick >= LED_PERIOD_MS) {
    led_last_tick += LED_PERIOD_MS;
    BSP_LED_Toggle(LED2);
  }

  if (now - sample_last_tick >= SAMPLE_PERIOD_MS) {
    sample_last_tick += SAMPLE_PERIOD_MS;

    /* Retry init each period so a loose wire recovers without a reset. After a
       successful init, skip this period's read: the first measurement isn't ready yet. */
    if (sensor_ok) {
      sensor_report();
    } else {
      sensor_ok = bme280_init(&sensor, app_i2c) == BME280_OK;
      if (!sensor_ok)
        sensor_check();
    }
  }
}

