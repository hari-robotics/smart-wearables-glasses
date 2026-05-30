#ifndef BIO_SENSORS_DRIVERS_MAX30205_CONVERT_H
#define BIO_SENSORS_DRIVERS_MAX30205_CONVERT_H

/**
 * \file MAX30205_convert.h
 * \brief Data conversion helpers for the MAX30205 thermometer.
 *
 * This header contains raw-byte conversion helpers and temperature-domain
 * constants that are useful when translating register values to degrees
 * Celsius and back.
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/* Temperature conversion behavior                                            */
/* -------------------------------------------------------------------------- */

#define MAX30205_TEMP_RESOLUTION_C_PER_LSB 0.00390625f
#define MAX30205_TEMP_LSB_PER_C 256.0f
#define MAX30205_EXTENDED_FORMAT_OFFSET_C 64.0f

#define MAX30205_TEMP_NORMAL_MIN_C 0.0f
#define MAX30205_TEMP_NORMAL_MAX_C 50.0f
#define MAX30205_TEMP_EXTENDED_MIN_C -64.0f
#define MAX30205_TEMP_EXTENDED_MAX_C 191.99609375f

/* -------------------------------------------------------------------------- */
/* Raw helpers                                                                */
/* -------------------------------------------------------------------------- */

#define MAX30205_RAW_MSB(raw) ((uint8_t)(((uint16_t)(raw) >> 8) & 0xFFU))
#define MAX30205_RAW_LSB(raw) ((uint8_t)((uint16_t)(raw) & 0xFFU))
#define MAX30205_RAW_FROM_BYTES(msb, lsb) \
  ((int16_t)((((uint16_t)(msb)) << 8) | ((uint16_t)(lsb))))

/* -------------------------------------------------------------------------- */
/* Inline conversion helpers                                                  */
/* -------------------------------------------------------------------------- */

static inline float MAX30205_RawToCelsius(int16_t raw_value,
                                          uint8_t extended_format) {
  float temperature_c = ((float)raw_value) * MAX30205_TEMP_RESOLUTION_C_PER_LSB;

  if (extended_format != 0U) {
    temperature_c += MAX30205_EXTENDED_FORMAT_OFFSET_C;
  }

  return temperature_c;
}

static inline int16_t MAX30205_CelsiusToRaw(float temperature_c,
                                            uint8_t extended_format) {
  float scaled_value;

  if (extended_format != 0U) {
    temperature_c -= MAX30205_EXTENDED_FORMAT_OFFSET_C;
  }

  scaled_value = temperature_c * MAX30205_TEMP_LSB_PER_C;

  if (scaled_value >= 0.0f) {
    scaled_value += 0.5f;
  } else {
    scaled_value -= 0.5f;
  }

  return (int16_t)scaled_value;
}

static inline float MAX30205_BytesToCelsius(uint8_t msb, uint8_t lsb,
                                            uint8_t extended_format) {
  return MAX30205_RawToCelsius(MAX30205_RAW_FROM_BYTES(msb, lsb),
                               extended_format);
}

static inline void MAX30205_RawToBytes(int16_t raw_value, uint8_t* msb,
                                       uint8_t* lsb) {
  if (msb != 0) {
    *msb = MAX30205_RAW_MSB(raw_value);
  }

  if (lsb != 0) {
    *lsb = MAX30205_RAW_LSB(raw_value);
  }
}

static inline void MAX30205_CelsiusToBytes(float temperature_c,
                                           uint8_t extended_format,
                                           uint8_t* msb, uint8_t* lsb) {
  MAX30205_RawToBytes(MAX30205_CelsiusToRaw(temperature_c, extended_format),
                      msb, lsb);
}

#ifdef __cplusplus
}
#endif

#endif  // BIO_SENSORS_DRIVERS_MAX30205_CONVERT_H
