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
