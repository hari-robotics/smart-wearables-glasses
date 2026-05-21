#ifndef BIO_SENSORS_DRIVERS_MAX30101_CONFIG_H
#define BIO_SENSORS_DRIVERS_MAX30101_CONFIG_H

/**
 * \file MAX30101_config.h
 * \brief App-facing configuration definitions for the MAX30101 biosensor.
 *
 * This header collects the configuration values that an app or driver is most
 * likely to tune directly: I2C addressing, FIFO behavior, mode selection,
 * SpO2 setup, LED currents, interrupt enables, and multi-LED slot routing.
 */

#include <stdint.h>

#include "bio_sensors/drivers/MAX30101_reg.h"
#include "main.h"

/* -------------------------------------------------------------------------- */
/* I2C settings                                                               */
/* -------------------------------------------------------------------------- */

/* 7-bit slave address: 0b1010111 */
#define MAX30101_I2C_ADDRESS 0x57U
#define MAX30101_I2C_WRITE_ADDRESS 0xAEU
#define MAX30101_I2C_READ_ADDRESS 0xAFU
#define MAX30101_I2C_TIMEOUT_MS I2C_TIMEOUT

/* -------------------------------------------------------------------------- */
/* Generic field masks for configuration registers                            */
/* -------------------------------------------------------------------------- */

#define MAX30101_FIFO_ALMOST_FULL_MASK 0x0FU
#define MAX30101_MODE_MASK 0x07U
#define MAX30101_SPO2_ADC_RANGE_MASK 0x60U
#define MAX30101_SPO2_SAMPLE_RATE_MASK 0x1CU
#define MAX30101_LED_PULSE_WIDTH_MASK 0x03U
#define MAX30101_SLOT_MASK 0x07U

/* -------------------------------------------------------------------------- */
/* Interrupt bits                                                             */
/* -------------------------------------------------------------------------- */

#define MAX30101_INT_A_FULL (1U << 7)
#define MAX30101_INT_PPG_RDY (1U << 6)
#define MAX30101_INT_ALC_OVF (1U << 5)
#define MAX30101_INT_PWR_RDY (1U << 0)
#define MAX30101_INT_DIE_TEMP_RDY (1U << 1)

#define MAX30101_INT_EN_A_FULL MAX30101_INT_A_FULL
#define MAX30101_INT_EN_PPG_RDY MAX30101_INT_PPG_RDY
#define MAX30101_INT_EN_ALC_OVF MAX30101_INT_ALC_OVF
#define MAX30101_INT_EN_DIE_TEMP_RDY MAX30101_INT_DIE_TEMP_RDY

/* -------------------------------------------------------------------------- */
/* FIFO configuration register (0x08)                                         */
/* -------------------------------------------------------------------------- */

#define MAX30101_FIFO_SAMPLE_AVG_1 (0x00U << 5)
#define MAX30101_FIFO_SAMPLE_AVG_2 (0x01U << 5)
#define MAX30101_FIFO_SAMPLE_AVG_4 (0x02U << 5)
#define MAX30101_FIFO_SAMPLE_AVG_8 (0x03U << 5)
#define MAX30101_FIFO_SAMPLE_AVG_16 (0x04U << 5)
#define MAX30101_FIFO_SAMPLE_AVG_32 (0x05U << 5)

#define MAX30101_FIFO_ROLLOVER_EN (1U << 4)
#define MAX30101_FIFO_ROLLOVER_DIS 0x00U

/* FIFO almost-full threshold. 0 means interrupt when 32 unread samples exist.
 */
#define MAX30101_FIFO_A_FULL_32_SAMPLES 0x00U
#define MAX30101_FIFO_A_FULL_31_SAMPLES 0x01U
#define MAX30101_FIFO_A_FULL_30_SAMPLES 0x02U
#define MAX30101_FIFO_A_FULL_29_SAMPLES 0x03U
#define MAX30101_FIFO_A_FULL_28_SAMPLES 0x04U
#define MAX30101_FIFO_A_FULL_27_SAMPLES 0x05U
#define MAX30101_FIFO_A_FULL_26_SAMPLES 0x06U
#define MAX30101_FIFO_A_FULL_25_SAMPLES 0x07U
#define MAX30101_FIFO_A_FULL_24_SAMPLES 0x08U
#define MAX30101_FIFO_A_FULL_23_SAMPLES 0x09U
#define MAX30101_FIFO_A_FULL_22_SAMPLES 0x0AU
#define MAX30101_FIFO_A_FULL_21_SAMPLES 0x0BU
#define MAX30101_FIFO_A_FULL_20_SAMPLES 0x0CU
#define MAX30101_FIFO_A_FULL_19_SAMPLES 0x0DU
#define MAX30101_FIFO_A_FULL_18_SAMPLES 0x0EU
#define MAX30101_FIFO_A_FULL_17_SAMPLES 0x0FU

/* -------------------------------------------------------------------------- */
/* Mode configuration register (0x09)                                         */
/* -------------------------------------------------------------------------- */

#define MAX30101_MODE_SHUTDOWN (1U << 7)
#define MAX30101_MODE_RESET (1U << 6)

#define MAX30101_MODE_NONE_0 0x00U
#define MAX30101_MODE_NONE_1 0x01U
#define MAX30101_MODE_HEART_RATE 0x02U
#define MAX30101_MODE_SPO2 0x03U
#define MAX30101_MODE_MULTI_LED 0x07U

/* -------------------------------------------------------------------------- */
/* SpO2 configuration register (0x0A)                                         */
/* -------------------------------------------------------------------------- */

#define MAX30101_ADC_RANGE_2048NA (0x00U << 5)
#define MAX30101_ADC_RANGE_4096NA (0x01U << 5)
#define MAX30101_ADC_RANGE_8192NA (0x02U << 5)
#define MAX30101_ADC_RANGE_16384NA (0x03U << 5)

typedef enum {
  SPS50 = 0x00U,
  SPS100 = 0x01U,
  SPS200 = 0x02U,
  SPS400 = 0x03U,
  SPS800 = 0x04U,
  SPS1000 = 0x05U,
  SPS1600 = 0x06U,
  SPS3200 = 0x07U
} max30101_sample_rate_t;

#define MAX30101_SAMPLE_RATE(sample_rate) \
  ((max30101_sample_rate_t)(sample_rate) << 2)

#define MAX30101_SAMPLE_RATE_50_SPS (0x00U << 2)
#define MAX30101_SAMPLE_RATE_100_SPS (0x01U << 2)
#define MAX30101_SAMPLE_RATE_200_SPS (0x02U << 2)
#define MAX30101_SAMPLE_RATE_400_SPS (0x03U << 2)
#define MAX30101_SAMPLE_RATE_800_SPS (0x04U << 2)
#define MAX30101_SAMPLE_RATE_1000_SPS (0x05U << 2)
#define MAX30101_SAMPLE_RATE_1600_SPS (0x06U << 2)
#define MAX30101_SAMPLE_RATE_3200_SPS (0x07U << 2)

#define MAX30101_PULSE_WIDTH_69_US 0x00U
#define MAX30101_PULSE_WIDTH_118_US 0x01U
#define MAX30101_PULSE_WIDTH_215_US 0x02U
#define MAX30101_PULSE_WIDTH_411_US 0x03U

#define MAX30101_ADC_RESOLUTION_15_BIT MAX30101_PULSE_WIDTH_69_US
#define MAX30101_ADC_RESOLUTION_16_BIT MAX30101_PULSE_WIDTH_118_US
#define MAX30101_ADC_RESOLUTION_17_BIT MAX30101_PULSE_WIDTH_215_US
#define MAX30101_ADC_RESOLUTION_18_BIT MAX30101_PULSE_WIDTH_411_US

/* -------------------------------------------------------------------------- */
/* LED pulse amplitude registers (0x0C-0x0F)                                  */
/* -------------------------------------------------------------------------- */

/* Approximate step size is 0.2mA/LSB, with 0xFF ~= 51mA typical. */
#define MAX30101_LED_CURRENT_OFF 0x00U
#define MAX30101_LED_CURRENT_3MA 0x0FU
#define MAX30101_LED_CURRENT_6_2MA 0x1FU
#define MAX30101_LED_CURRENT_12_6MA 0x3FU
#define MAX30101_LED_CURRENT_25_4MA 0x7FU
#define MAX30101_LED_CURRENT_51MA 0xFFU

/* --------------------------------------------------------------------------
 */
/* Multi-LED control registers (0x11-0x12) */
/* --------------------------------------------------------------------------
 */

#define MAX30101_SLOT1_SHIFT 0U
#define MAX30101_SLOT2_SHIFT 4U
#define MAX30101_SLOT3_SHIFT 0U
#define MAX30101_SLOT4_SHIFT 4U

#define MAX30101_SLOT_DISABLED 0x00U
#define MAX30101_SLOT_RED_LED 0x01U
#define MAX30101_SLOT_IR_LED 0x02U
#define MAX30101_SLOT_GREEN_LED 0x03U

#define MAX30101_SLOT_VALUE(slot, shift) \
  ((((uint8_t)(slot)) & MAX30101_SLOT_MASK) << (shift))

/* --------------------------------------------------------------------------
 */
/* Temperature configuration register (0x21) */
/* --------------------------------------------------------------------------
 */

#define MAX30101_TEMP_ENABLE (1U << 0)

typedef struct {
  uint8_t fifo_config;
  uint8_t mode;
  uint8_t spo2_config;
  uint8_t red_led_current_pa1;
  uint8_t ir_led_current_pa2;
  uint8_t int_enable_1;
  uint8_t int_enable_2;
  uint8_t multi_led_ctrl1;
  uint8_t multi_led_ctrl2;
} max30101_config_t;

#endif  // BIO_SENSORS_DRIVERS_MAX30101_CONFIG_H
