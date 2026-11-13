#include "bme280_comp.h"

/* Calibration words are little-endian: low byte at the lower address. */
static uint16_t u16_le(const uint8_t *p) { return (uint16_t)(p[1] << 8 | p[0]); }
static int16_t  s16_le(const uint8_t *p) { return (int16_t)u16_le(p); }

/* c1[] is 0x88..0xA1, c2[] is 0xE1..0xE7 (datasheet table 16). */
void bme280_parse_calib(const uint8_t c1[BME280_CALIB1_LEN],
                        const uint8_t c2[BME280_CALIB2_LEN],
                        bme280_calib_t *out)
{
  out->dig_T1 = u16_le(&c1[0]);    /* 0x88 */
  out->dig_T2 = s16_le(&c1[2]);    /* 0x8A */
  out->dig_T3 = s16_le(&c1[4]);    /* 0x8C */

  out->dig_P1 = u16_le(&c1[6]);    /* 0x8E */
  out->dig_P2 = s16_le(&c1[8]);    /* 0x90 */
  out->dig_P3 = s16_le(&c1[10]);
  out->dig_P4 = s16_le(&c1[12]);
  out->dig_P5 = s16_le(&c1[14]);
  out->dig_P6 = s16_le(&c1[16]);
  out->dig_P7 = s16_le(&c1[18]);
  out->dig_P8 = s16_le(&c1[20]);
  out->dig_P9 = s16_le(&c1[22]);   /* 0x9E */
  /* c1[24] (0xA0) is unused */
  out->dig_H1 = c1[25];            /* 0xA1 */

  out->dig_H2 = s16_le(&c2[0]);    /* 0xE1 */
  out->dig_H3 = c2[2];             /* 0xE3 */
  /* H4 and H5 are signed 12-bit, sharing 0xE5:
       H4 = 0xE4[7:0] : 0xE5[3:0]
       H5 = 0xE6[7:0] : 0xE5[7:4]
     The MSB byte carries the sign, so it is sign-extended before scaling by 16. */
  out->dig_H4 = (int16_t)((int8_t)c2[3] * 16 | (c2[4] & 0x0F));
  out->dig_H5 = (int16_t)((int8_t)c2[5] * 16 | (c2[4] >> 4));
  out->dig_H6 = (int8_t)c2[6];     /* 0xE7 */
}

/* d[] is the burst read of 0xF7..0xFE:
     press_msb, press_lsb, press_xlsb, temp_msb, temp_lsb, temp_xlsb, hum_msb, hum_lsb
   Pressure and temperature are 20-bit, with the low 4 bits in xlsb[7:4]. */
void bme280_parse_raw(const uint8_t d[BME280_RAW_LEN], bme280_raw_t *out)
{
  out->adc_P = ((int32_t)d[0] << 12) | ((int32_t)d[1] << 4) | (d[2] >> 4);
  out->adc_T = ((int32_t)d[3] << 12) | ((int32_t)d[4] << 4) | (d[5] >> 4);
  out->adc_H = ((int32_t)d[6] << 8)  |  d[7];
}

/* Compensation formulas from the datasheet (section 4.2.3), integer versions.
   t_fine carries the temperature into the pressure and humidity formulas.
   Left shifts of possibly negative values are written as multiplies to avoid UB. */
static int32_t comp_temp(const bme280_calib_t *c, int32_t adc_T, int32_t *t_fine)
{
  int32_t var1 = ((((adc_T >> 3) - ((int32_t)c->dig_T1 << 1))) * c->dig_T2) >> 11;
  int32_t var2 = (((((adc_T >> 4) - c->dig_T1) * ((adc_T >> 4) - c->dig_T1)) >> 12) *
                  c->dig_T3) >> 14;
  *t_fine = var1 + var2;
  return (*t_fine * 5 + 128) >> 8;              /* 0.01 °C */
}

/* Returns Pa in Q24.8 (value / 256 = Pa). */
static uint32_t comp_press(const bme280_calib_t *c, int32_t adc_P, int32_t t_fine)
{
  int64_t var1 = (int64_t)t_fine - 128000;
  int64_t var2 = var1 * var1 * c->dig_P6;
  var2 += (var1 * c->dig_P5) * (1LL << 17);
  var2 += (int64_t)c->dig_P4 * (1LL << 35);
  var1 = ((var1 * var1 * c->dig_P3) >> 8) + ((var1 * c->dig_P2) * (1LL << 12));
  var1 = (((1LL << 47) + var1) * c->dig_P1) >> 33;
  if (var1 == 0)
    return 0;                                   /* avoid divide by zero */

  int64_t p = 1048576 - adc_P;
  p = ((p * (1LL << 31)) - var2) * 3125 / var1;
  var1 = ((int64_t)c->dig_P9 * (p >> 13) * (p >> 13)) >> 25;
  var2 = ((int64_t)c->dig_P8 * p) >> 19;
  p = ((p + var1 + var2) >> 8) + ((int64_t)c->dig_P7 * 16);
  return (uint32_t)p;
}

/* Returns %RH in Q22.10 (value / 1024 = %RH). */
static uint32_t comp_hum(const bme280_calib_t *c, int32_t adc_H, int32_t t_fine)
{
  int32_t v = t_fine - 76800;
  v = (((((adc_H << 14) - (c->dig_H4 * (1 << 20)) - (c->dig_H5 * v)) + 16384) >> 15) *
       (((((((v * c->dig_H6) >> 10) * (((v * c->dig_H3) >> 11) + 32768)) >> 10) + 2097152) *
         c->dig_H2 + 8192) >> 14));
  v -= ((((v >> 15) * (v >> 15)) >> 7) * c->dig_H1) >> 4;
  if (v < 0)         v = 0;
  if (v > 419430400) v = 419430400;             /* 100 %RH */
  return (uint32_t)(v >> 12);
}

void bme280_compensate(const bme280_calib_t *cal, const bme280_raw_t *raw,
                       bme280_meas_t *out)
{
  int32_t t_fine;

  out->temp_centi_c  = comp_temp(cal, raw->adc_T, &t_fine);
  out->press_pa      = (comp_press(cal, raw->adc_P, t_fine) + 128) >> 8;
  out->hum_milli_pct = (comp_hum(cal, raw->adc_H, t_fine) * 1000u) >> 10;
}
