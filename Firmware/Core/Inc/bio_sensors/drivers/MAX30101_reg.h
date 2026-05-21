#ifndef __BIO_SENSOR_H__
#define __BIO_SENSOR_H__

/**
 * \file bio_sensor.h
 * \brief Register map and raw data layout definitions for the MAX30101.
 *
 * App-facing configuration values live in MAX30101_config.h so that users can
 * find mode, FIFO, LED, and interrupt settings without searching through the
 * register table.
 */

/* -------------------------------------------------------------------------- */
/* Device identification                                                      */
/* -------------------------------------------------------------------------- */

#define MAX30101_EXPECTED_PART_ID 0x15U

/* -------------------------------------------------------------------------- */
/* Register map                                                               */
/* -------------------------------------------------------------------------- */

#define MAX30101_REG_INT_STATUS_1 0x00U
#define MAX30101_REG_INT_STATUS_2 0x01U
#define MAX30101_REG_INT_ENABLE_1 0x02U
#define MAX30101_REG_INT_ENABLE_2 0x03U
#define MAX30101_REG_FIFO_WR_PTR 0x04U
#define MAX30101_REG_OVF_COUNTER 0x05U
#define MAX30101_REG_FIFO_RD_PTR 0x06U
#define MAX30101_REG_FIFO_DATA 0x07U
#define MAX30101_REG_FIFO_CONFIG 0x08U
#define MAX30101_REG_MODE_CONFIG 0x09U
#define MAX30101_REG_SPO2_CONFIG 0x0AU
#define MAX30101_REG_RESERVED_0B 0x0BU
#define MAX30101_REG_LED1_PA 0x0CU
#define MAX30101_REG_LED2_PA 0x0DU
#define MAX30101_REG_LED3_PA 0x0EU
#define MAX30101_REG_LED4_PA 0x0FU
#define MAX30101_REG_MULTI_LED_CTRL1 0x11U
#define MAX30101_REG_MULTI_LED_CTRL2 0x12U
#define MAX30101_REG_TEMP_INT 0x1FU
#define MAX30101_REG_TEMP_FRAC 0x20U
#define MAX30101_REG_TEMP_CONFIG 0x21U
#define MAX30101_REG_REV_ID 0xFEU
#define MAX30101_REG_PART_ID 0xFFU

/* -------------------------------------------------------------------------- */
/* Raw data and FIFO helpers                                                  */
/* -------------------------------------------------------------------------- */

#define MAX30101_FIFO_POINTER_MASK 0x1FU
#define MAX30101_FIFO_DATA_MASK 0x3FFFFUL

/* -------------------------------------------------------------------------- */
/* FIFO and sample sizing helpers                                             */
/* -------------------------------------------------------------------------- */

#define MAX30101_FIFO_DEPTH_SAMPLES 32U
#define MAX30101_FIFO_BYTES_PER_CHANNEL 3U
#define MAX30101_FIFO_CHANNELS_HR_MODE 1U
#define MAX30101_FIFO_CHANNELS_SPO2_MODE 2U
#define MAX30101_FIFO_MAX_ACTIVE_SLOTS 4U

#define MAX30101_FIFO_BYTES_PER_HR_SAMPLE \
  (MAX30101_FIFO_BYTES_PER_CHANNEL * MAX30101_FIFO_CHANNELS_HR_MODE)
#define MAX30101_FIFO_BYTES_PER_SPO2_SAMPLE \
  (MAX30101_FIFO_BYTES_PER_CHANNEL * MAX30101_FIFO_CHANNELS_SPO2_MODE)
#define MAX30101_FIFO_BYTES_PER_MAX_MULTI_SAMPLE \
  (MAX30101_FIFO_BYTES_PER_CHANNEL * MAX30101_FIFO_MAX_ACTIVE_SLOTS)

#endif /* __BIO_SENSOR_H__ */
