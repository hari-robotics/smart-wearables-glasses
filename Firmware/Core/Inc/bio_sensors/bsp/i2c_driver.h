#ifndef BIO_SENSORS_DRIVER_CONFIG_H
#define BIO_SENSORS_DRIVER_CONFIG_H

#include <stdint.h>

#include "stm32u5xx_hal.h"
namespace bio_sensors::peripherals {

typedef enum { OK, BUSY, TIMEOUT, ERROR } i2c_status_t;

class I2C {
 public:
  I2C(I2C_HandleTypeDef* handle);
  i2c_status_t read(uint8_t dev_addr, uint8_t reg_addr, uint8_t* p_buff,
                    uint16_t len);
  i2c_status_t read(uint8_t dev_addr, uint8_t reg_addr, uint8_t& buff);
  i2c_status_t write(uint8_t dev_addr, uint8_t reg_addr, uint8_t* p_buff,
                     uint16_t len);
  i2c_status_t write(uint8_t dev_addr, uint8_t reg_addr, uint8_t& buff);

 private:
  I2C_HandleTypeDef* i2c_handle;
};

// Type conversion functions (For BSP compatibility)
i2c_status_t convertTypesST(HAL_StatusTypeDef st_status);

}  // namespace bio_sensors::peripherals

#endif
