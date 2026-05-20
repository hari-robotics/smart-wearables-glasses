#ifndef BIO_SENSORS_DRIVER_CONFIG_H
#define BIO_SENSORS_DRIVER_CONFIG_H

#include <stdint.h>

#include "stm32u5xx_hal.h"
namespace bio_sensors::i2c {

typedef enum { OK, BUSY, TIMEOUT, ERROR } status_t;

class Peripheral {
 public:
  Peripheral(I2C_HandleTypeDef* handle);
  status_t read(uint8_t dev_addr, uint8_t reg_addr, uint8_t* p_buff,
                uint16_t len);
  status_t read(uint8_t dev_addr, uint8_t reg_addr, uint8_t& buff);
  status_t write(uint8_t dev_addr, uint8_t reg_addr, uint8_t* p_buff,
                 uint16_t len);
  status_t write(uint8_t dev_addr, uint8_t reg_addr, uint8_t& buff);

 private:
  I2C_HandleTypeDef*
      i2c_handle;  // Default to hi2c3, can be overridden if needed
};

// Type conversion functions (For BSP compatibility)
status_t convertTypesST(HAL_StatusTypeDef st_status);

}  // namespace bio_sensors::i2c

#endif
