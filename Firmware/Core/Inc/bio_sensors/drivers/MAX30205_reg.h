#ifndef __MAX30205_H__
#define __MAX30205_H__

/**
 * \file max30205.h
 * \brief Register map and register metadata for the MAX30205.
 *
 * App-facing configuration values live in MAX30205_config.h so that users can
 * find I2C addresses, config-bit selections, and conversion helpers without
 * searching through the register map.
 */

/* -------------------------------------------------------------------------- */
/* Register pointer values                                                    */
/* -------------------------------------------------------------------------- */

#define MAX30205_REG_TEMPERATURE 0x00U
#define MAX30205_REG_CONFIGURATION 0x01U
#define MAX30205_REG_THYST 0x02U
#define MAX30205_REG_TOS 0x03U

#define MAX30205_REGISTER_COUNT 4U
#define MAX30205_POINTER_MASK 0x03U

/* -------------------------------------------------------------------------- */
/* Register sizes and power-on defaults                                       */
/* -------------------------------------------------------------------------- */

#define MAX30205_TEMPERATURE_REGISTER_SIZE_BYTES 2U
#define MAX30205_CONFIGURATION_REGISTER_SIZE_BYTES 1U
#define MAX30205_THRESHOLD_REGISTER_SIZE_BYTES 2U

#define MAX30205_POR_TEMPERATURE_RAW 0x0000U
#define MAX30205_POR_CONFIGURATION 0x00U
#define MAX30205_POR_THYST_RAW 0x4B00U
#define MAX30205_POR_TOS_RAW 0x5000U

#define MAX30205_POR_THYST_C 75.0f
#define MAX30205_POR_TOS_C 80.0f

#endif /* __MAX30205_H__ */
