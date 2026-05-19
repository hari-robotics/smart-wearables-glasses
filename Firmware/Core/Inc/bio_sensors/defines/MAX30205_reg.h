#ifndef __MAX30205_H__
#define __MAX30205_H__

/**
 * \file max30205.h
 * \brief Register definitions and helper operations for the MAX30205 temperature sensor.
 *
 * This header provides the complete MAX30205 register map, configuration bit
 * definitions, address options, and lightweight helper functions for
 * converting raw register data to temperature values and back.
 */

#include "stdint.h"
#include "main.h"

/* -------------------------------------------------------------------------- */
/* I2C timing and resolution                                                  */
/* -------------------------------------------------------------------------- */

#define MAX30205_I2C_TIMEOUT_MS                   I2C_TIMEOUT
#define MAX30205_I2C_MAX_FREQ_HZ                  400000U

#define MAX30205_TEMP_RESOLUTION_C_PER_LSB        0.00390625f
#define MAX30205_TEMP_LSB_PER_C                   256.0f
#define MAX30205_EXTENDED_FORMAT_OFFSET_C         64.0f

#define MAX30205_CONVERSION_TIME_TYP_MS           44U
#define MAX30205_CONVERSION_TIME_MAX_MS           50U

#define MAX30205_BUS_TIMEOUT_TYP_MS               50U
#define MAX30205_BUS_TIMEOUT_MIN_MS               45U
#define MAX30205_BUS_TIMEOUT_MAX_MS               55U

/* -------------------------------------------------------------------------- */
/* Address selection                                                          */
/* -------------------------------------------------------------------------- */

/*
 * Datasheet Table 1 lists 8-bit slave address bytes. The corresponding
 * 7-bit address equals (address_byte >> 1).
 */
#define MAX30205_I2C_ADDR_WRITE_A2_GND_A1_GND_A0_GND   0x90U
#define MAX30205_I2C_ADDR_WRITE_A2_GND_A1_GND_A0_VDD   0x92U
#define MAX30205_I2C_ADDR_WRITE_A2_GND_A1_GND_A0_SCL   0x82U
#define MAX30205_I2C_ADDR_WRITE_A2_GND_A1_GND_A0_SDA   0x80U
#define MAX30205_I2C_ADDR_WRITE_A2_GND_A1_VDD_A0_GND   0x94U
#define MAX30205_I2C_ADDR_WRITE_A2_GND_A1_VDD_A0_VDD   0x96U
#define MAX30205_I2C_ADDR_WRITE_A2_GND_A1_VDD_A0_SCL   0x86U
#define MAX30205_I2C_ADDR_WRITE_A2_GND_A1_VDD_A0_SDA   0x84U
#define MAX30205_I2C_ADDR_WRITE_A2_GND_A1_SCL_A0_GND   0xB4U
#define MAX30205_I2C_ADDR_WRITE_A2_GND_A1_SCL_A0_VDD   0xB6U
#define MAX30205_I2C_ADDR_WRITE_A2_GND_A1_SCL_A0_SCL   0xA6U
#define MAX30205_I2C_ADDR_WRITE_A2_GND_A1_SCL_A0_SDA   0xA4U
#define MAX30205_I2C_ADDR_WRITE_A2_GND_A1_SDA_A0_GND   0xB0U
#define MAX30205_I2C_ADDR_WRITE_A2_GND_A1_SDA_A0_VDD   0xB2U
#define MAX30205_I2C_ADDR_WRITE_A2_GND_A1_SDA_A0_SCL   0xA2U
#define MAX30205_I2C_ADDR_WRITE_A2_GND_A1_SDA_A0_SDA   0xA0U
#define MAX30205_I2C_ADDR_WRITE_A2_VDD_A1_GND_A0_GND   0x98U
#define MAX30205_I2C_ADDR_WRITE_A2_VDD_A1_GND_A0_VDD   0x9AU
#define MAX30205_I2C_ADDR_WRITE_A2_VDD_A1_GND_A0_SCL   0x8AU
#define MAX30205_I2C_ADDR_WRITE_A2_VDD_A1_GND_A0_SDA   0x88U
#define MAX30205_I2C_ADDR_WRITE_A2_VDD_A1_VDD_A0_GND   0x9CU
#define MAX30205_I2C_ADDR_WRITE_A2_VDD_A1_VDD_A0_VDD   0x9EU
#define MAX30205_I2C_ADDR_WRITE_A2_VDD_A1_VDD_A0_SCL   0x8EU
#define MAX30205_I2C_ADDR_WRITE_A2_VDD_A1_VDD_A0_SDA   0x8CU
#define MAX30205_I2C_ADDR_WRITE_A2_VDD_A1_SCL_A0_GND   0xBCU
#define MAX30205_I2C_ADDR_WRITE_A2_VDD_A1_SCL_A0_VDD   0xBEU
#define MAX30205_I2C_ADDR_WRITE_A2_VDD_A1_SCL_A0_SCL   0xAEU
#define MAX30205_I2C_ADDR_WRITE_A2_VDD_A1_SCL_A0_SDA   0xACU
#define MAX30205_I2C_ADDR_WRITE_A2_VDD_A1_SDA_A0_GND   0xB8U
#define MAX30205_I2C_ADDR_WRITE_A2_VDD_A1_SDA_A0_VDD   0xBAU
#define MAX30205_I2C_ADDR_WRITE_A2_VDD_A1_SDA_A0_SCL   0xAAU
#define MAX30205_I2C_ADDR_WRITE_A2_VDD_A1_SDA_A0_SDA   0xA8U

#define MAX30205_I2C_DEFAULT_WRITE_ADDRESS           MAX30205_I2C_ADDR_WRITE_A2_GND_A1_GND_A0_GND
#define MAX30205_I2C_DEFAULT_READ_ADDRESS            (MAX30205_I2C_DEFAULT_WRITE_ADDRESS | 0x01U)
#define MAX30205_I2C_DEFAULT_7BIT_ADDRESS            (MAX30205_I2C_DEFAULT_WRITE_ADDRESS >> 1)

#define MAX30205_I2C_READ_ADDRESS_FROM_WRITE(addr)   ((uint8_t)((addr) | 0x01U))
#define MAX30205_I2C_7BIT_ADDRESS(addr)              ((uint8_t)((addr) >> 1))

/* -------------------------------------------------------------------------- */
/* Register pointer values                                                    */
/* -------------------------------------------------------------------------- */

#define MAX30205_REG_TEMPERATURE                     0x00U
#define MAX30205_REG_CONFIGURATION                   0x01U
#define MAX30205_REG_THYST                           0x02U
#define MAX30205_REG_TOS                             0x03U

#define MAX30205_REGISTER_COUNT                      4U
#define MAX30205_POINTER_MASK                        0x03U

/* -------------------------------------------------------------------------- */
/* Register sizes and power-on defaults                                       */
/* -------------------------------------------------------------------------- */

#define MAX30205_TEMPERATURE_REGISTER_SIZE_BYTES     2U
#define MAX30205_CONFIGURATION_REGISTER_SIZE_BYTES   1U
#define MAX30205_THRESHOLD_REGISTER_SIZE_BYTES       2U

#define MAX30205_POR_TEMPERATURE_RAW                 0x0000U
#define MAX30205_POR_CONFIGURATION                   0x00U
#define MAX30205_POR_THYST_RAW                       0x4B00U
#define MAX30205_POR_TOS_RAW                         0x5000U

#define MAX30205_POR_THYST_C                         75.0f
#define MAX30205_POR_TOS_C                           80.0f

/* -------------------------------------------------------------------------- */
/* Configuration register (0x01)                                              */
/* -------------------------------------------------------------------------- */

#define MAX30205_CONFIG_SHUTDOWN_MASK                (1U << 0)
#define MAX30205_CONFIG_OS_MODE_MASK                 (1U << 1)
#define MAX30205_CONFIG_OS_POLARITY_MASK             (1U << 2)
#define MAX30205_CONFIG_FAULT_QUEUE_MASK             (3U << 3)
#define MAX30205_CONFIG_DATA_FORMAT_MASK             (1U << 5)
#define MAX30205_CONFIG_TIMEOUT_MASK                 (1U << 6)
#define MAX30205_CONFIG_ONE_SHOT_MASK                (1U << 7)

#define MAX30205_CONFIG_CONTINUOUS                   0x00U
#define MAX30205_CONFIG_SHUTDOWN                     MAX30205_CONFIG_SHUTDOWN_MASK

#define MAX30205_CONFIG_OS_COMPARATOR                0x00U
#define MAX30205_CONFIG_OS_INTERRUPT                 MAX30205_CONFIG_OS_MODE_MASK

#define MAX30205_CONFIG_OS_ACTIVE_LOW                0x00U
#define MAX30205_CONFIG_OS_ACTIVE_HIGH               MAX30205_CONFIG_OS_POLARITY_MASK

#define MAX30205_CONFIG_FAULT_QUEUE_1                (0x00U << 3)
#define MAX30205_CONFIG_FAULT_QUEUE_2                (0x01U << 3)
#define MAX30205_CONFIG_FAULT_QUEUE_4                (0x02U << 3)
#define MAX30205_CONFIG_FAULT_QUEUE_6                (0x03U << 3)

#define MAX30205_CONFIG_FORMAT_NORMAL                0x00U
#define MAX30205_CONFIG_FORMAT_EXTENDED              MAX30205_CONFIG_DATA_FORMAT_MASK

#define MAX30205_CONFIG_TIMEOUT_ENABLE               0x00U
#define MAX30205_CONFIG_TIMEOUT_DISABLE              MAX30205_CONFIG_TIMEOUT_MASK

#define MAX30205_CONFIG_ONE_SHOT_START               MAX30205_CONFIG_ONE_SHOT_MASK

/* -------------------------------------------------------------------------- */
/* Temperature operating ranges                                               */
/* -------------------------------------------------------------------------- */

#define MAX30205_TEMP_NORMAL_MIN_C                   0.0f
#define MAX30205_TEMP_NORMAL_MAX_C                   50.0f
#define MAX30205_TEMP_EXTENDED_MIN_C                 -64.0f
#define MAX30205_TEMP_EXTENDED_MAX_C                 191.99609375f

/* -------------------------------------------------------------------------- */
/* Raw helpers                                                                */
/* -------------------------------------------------------------------------- */

#define MAX30205_RAW_MSB(raw)                        ((uint8_t)(((uint16_t)(raw) >> 8) & 0xFFU))
#define MAX30205_RAW_LSB(raw)                        ((uint8_t)((uint16_t)(raw) & 0xFFU))
#define MAX30205_RAW_FROM_BYTES(msb, lsb)            ((int16_t)((((uint16_t)(msb)) << 8) | ((uint16_t)(lsb))))

/* -------------------------------------------------------------------------- */
/* Inline conversion helpers                                                  */
/* -------------------------------------------------------------------------- */

static inline float MAX30205_RawToCelsius(int16_t raw_value, uint8_t extended_format)
{
    float temperature_c = ((float)raw_value) * MAX30205_TEMP_RESOLUTION_C_PER_LSB;

    if (extended_format != 0U) {
        temperature_c += MAX30205_EXTENDED_FORMAT_OFFSET_C;
    }

    return temperature_c;
}

static inline int16_t MAX30205_CelsiusToRaw(float temperature_c, uint8_t extended_format)
{
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

static inline float MAX30205_BytesToCelsius(uint8_t msb, uint8_t lsb, uint8_t extended_format)
{
    return MAX30205_RawToCelsius(MAX30205_RAW_FROM_BYTES(msb, lsb), extended_format);
}

static inline void MAX30205_RawToBytes(int16_t raw_value, uint8_t *msb, uint8_t *lsb)
{
    if (msb != 0) {
        *msb = MAX30205_RAW_MSB(raw_value);
    }

    if (lsb != 0) {
        *lsb = MAX30205_RAW_LSB(raw_value);
    }
}

static inline void MAX30205_CelsiusToBytes(float temperature_c, uint8_t extended_format, uint8_t *msb, uint8_t *lsb)
{
    MAX30205_RawToBytes(MAX30205_CelsiusToRaw(temperature_c, extended_format), msb, lsb);
}

static inline uint8_t MAX30205_BuildConfig(uint8_t one_shot,
                                           uint8_t timeout_disable,
                                           uint8_t extended_format,
                                           uint8_t fault_queue,
                                           uint8_t os_active_high,
                                           uint8_t interrupt_mode,
                                           uint8_t shutdown)
{
    return (uint8_t)(
        (one_shot ? MAX30205_CONFIG_ONE_SHOT_START : 0U) |
        (timeout_disable ? MAX30205_CONFIG_TIMEOUT_DISABLE : 0U) |
        (extended_format ? MAX30205_CONFIG_FORMAT_EXTENDED : 0U) |
        (fault_queue & MAX30205_CONFIG_FAULT_QUEUE_MASK) |
        (os_active_high ? MAX30205_CONFIG_OS_ACTIVE_HIGH : 0U) |
        (interrupt_mode ? MAX30205_CONFIG_OS_INTERRUPT : 0U) |
        (shutdown ? MAX30205_CONFIG_SHUTDOWN : 0U));
}

#endif /* __MAX30205_H__ */
