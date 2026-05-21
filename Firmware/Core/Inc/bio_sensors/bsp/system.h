#ifndef BIO_SENSORS_BSP_DELAY_H
#define BIO_SENSORS_BSP_DELAY_H

#include <cstdint>

#include "stm32u5xx_hal.h"
namespace bio_sensors::sys {

inline void delay(uint32_t millis) { HAL_Delay(millis); }
inline uint32_t time() { return HAL_GetTick(); }

}  // namespace bio_sensors::sys

#endif