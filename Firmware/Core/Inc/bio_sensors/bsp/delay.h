#ifndef BIO_SENSORS_BSP_DELAY_H
#define BIO_SENSORS_BSP_DELAY_H

#include "stm32u5xx_hal.h"
namespace bio_sensors::sys {
inline void delay(uint32_t millis) { HAL_Delay(millis); }
}  // namespace bio_sensors::sys

#endif