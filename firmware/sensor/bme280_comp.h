#ifndef BME280_COMP_H
#define BME280_COMP_H

#include <stdint.h>

#define BME280_CALIB1_LEN 26u   /* 0x88..0xA1 */
#define BME280_CALIB2_LEN 7u    /* 0xE1..0xE7 */
#define BME280_RAW_LEN    8u    /* 0xF7..0xFE */

typedef struct {
  uint16_t dig_T1;
  int16_t  dig_T2, dig_T3;
  uint16_t dig_P1;
  int16_t  dig_P2, dig_P3, dig_P4, dig_P5, dig_P6, dig_P7, dig_P8, dig_P9;
  uint8_t  dig_H1;
  int16_t  dig_H2;
  uint8_t  dig_H3;
  int16_t  dig_H4, dig_H5;
  int8_t   dig_H6;
} bme280_calib_t;

typedef struct {
  int32_t adc_T, adc_P, adc_H;
} bme280_raw_t;

typedef struct {
  int32_t  temp_centi_c;    /* 2350   = 23.50 °C    */
  uint32_t press_pa;        /* 101280 = 1012.80 hPa */
  uint32_t hum_milli_pct;   /* 42100  = 42.100 %RH  */
} bme280_meas_t;

void bme280_parse_calib(const uint8_t c1[BME280_CALIB1_LEN],
                        const uint8_t c2[BME280_CALIB2_LEN],
                        bme280_calib_t *out);

void bme280_parse_raw(const uint8_t d[BME280_RAW_LEN], bme280_raw_t *out);

void bme280_compensate(const bme280_calib_t *cal, const bme280_raw_t *raw,
                       bme280_meas_t *out);

#endif
