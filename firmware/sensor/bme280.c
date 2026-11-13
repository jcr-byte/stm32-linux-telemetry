#include "bme280.h"

#define REG_CALIB1    0x88u
#define REG_CHIP_ID   0xD0u
#define REG_RESET     0xE0u
#define REG_CALIB2    0xE1u
#define REG_CTRL_HUM  0xF2u
#define REG_STATUS    0xF3u
#define REG_CTRL_MEAS 0xF4u
#define REG_CONFIG    0xF5u

#define CHIP_ID          0x60u
#define RESET_CMD        0xB6u
#define STATUS_IM_UPDATE 0x01u   /* set while NVM calibration is copied to registers */

#define I2C_TIMEOUT_MS   100u
#define RESET_TIMEOUT_MS 10u

static HAL_StatusTypeDef reg_read(bme280_t *dev, uint8_t reg, uint8_t *buf, uint16_t len)
{
  return HAL_I2C_Mem_Read(dev->hi2c, dev->addr << 1, reg, I2C_MEMADD_SIZE_8BIT,
                          buf, len, I2C_TIMEOUT_MS);
}

static HAL_StatusTypeDef reg_write(bme280_t *dev, uint8_t reg, uint8_t val)
{
  return HAL_I2C_Mem_Write(dev->hi2c, dev->addr << 1, reg, I2C_MEMADD_SIZE_8BIT,
                           &val, 1, I2C_TIMEOUT_MS);
}

bme280_status_t bme280_init(bme280_t *dev, I2C_HandleTypeDef *hi2c)
{
  uint8_t id, status;
  uint8_t c1[BME280_CALIB1_LEN], c2[BME280_CALIB2_LEN];

  dev->hi2c = hi2c;
  dev->addr = 0;

  /* SDO pin selects 0x76 (GND) or 0x77 (VDDIO). */
  for (uint8_t addr = 0x76; addr <= 0x77; addr++) {
    if (HAL_I2C_IsDeviceReady(hi2c, addr << 1, 3, 10) == HAL_OK) {
      dev->addr = addr;
      break;
    }
  }
  if (dev->addr == 0)
    return BME280_ERR_NOT_FOUND;

  /* Check chip ID. */
  if (reg_read(dev, REG_CHIP_ID, &id, 1) != HAL_OK)
    return BME280_ERR_I2C;
  if (id != CHIP_ID)
    return BME280_ERR_CHIP_ID;

  /* Soft reset, then wait for the NVM copy to finish. */
  if (reg_write(dev, REG_RESET, RESET_CMD) != HAL_OK)
    return BME280_ERR_I2C;
  uint32_t start = HAL_GetTick();
  do {
    HAL_Delay(2);
    if (HAL_GetTick() - start > RESET_TIMEOUT_MS)
      return BME280_ERR_TIMEOUT;
  } while (reg_read(dev, REG_STATUS, &status, 1) != HAL_OK ||
           (status & STATUS_IM_UPDATE));

  /* Read in calibration data. */
  if (reg_read(dev, REG_CALIB1, c1, sizeof c1) != HAL_OK ||
      reg_read(dev, REG_CALIB2, c2, sizeof c2) != HAL_OK)
    return BME280_ERR_I2C;
  bme280_parse_calib(c1, c2, &dev->calib);

  /* Configure. Order matters:
       - config is only reliably written in sleep mode, so before ctrl_meas
       - ctrl_hum only takes effect after a ctrl_meas write */
  if (reg_write(dev, REG_CONFIG,    0xA0) != HAL_OK ||  /* standby 1000 ms, filter off */
      reg_write(dev, REG_CTRL_HUM,  0x01) != HAL_OK ||  /* humidity x1 */
      reg_write(dev, REG_CTRL_MEAS, 0x27) != HAL_OK)    /* temp x1, press x1, normal mode */
    return BME280_ERR_I2C;

  return BME280_OK;
}
