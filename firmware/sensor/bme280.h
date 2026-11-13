#ifndef BME280_H
#define BME280_H

#include "stm32f4xx_hal.h"
#include "bme280_comp.h"

typedef enum {
  BME280_OK = 0,
  BME280_ERR_NOT_FOUND,   /* no ACK at 0x76 or 0x77 */
  BME280_ERR_CHIP_ID,     /* responded, but ID != 0x60 */
  BME280_ERR_I2C,         /* transfer failed mid-operation */
  BME280_ERR_TIMEOUT,     /* reset / NVM copy didn't finish */
  BME280_ERR_NO_DATA,     /* no measurement yet, or humidity not enabled */
} bme280_status_t;

typedef struct {
  I2C_HandleTypeDef *hi2c;
  uint8_t            addr;   /* 7-bit: 0x76 or 0x77 */
  bme280_calib_t     calib;
} bme280_t;

bme280_status_t bme280_init(bme280_t *dev, I2C_HandleTypeDef *hi2c);
bme280_status_t bme280_read(bme280_t *dev, bme280_meas_t *out);

#endif
