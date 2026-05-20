#include "i2c_driver.h"

#include <complex.h>
#include <stdint.h>

#include "main.h"
#include "peripherals.h"
#include "stm32u575xx.h"
#include "stm32u5xx_hal_i2c.h"

// Import ST I2C Handle
extern "C" I2C_HandleTypeDef hi2c3;
namespace bio_sensors::i2c {
status_t convertTypesST(HAL_StatusTypeDef st_status) {
  switch (st_status) {
    case HAL_OK:
      return status_t::OK;
    case HAL_BUSY:
      return status_t::BUSY;
    case HAL_TIMEOUT:
      return status_t::TIMEOUT;
    case HAL_ERROR:
      return status_t::ERROR;
    default:
      return status_t::ERROR;
  }
}

Peripheral::Peripheral(I2C_HandleTypeDef* handle) : i2c_handle(handle) {}

status_t Peripheral::read(uint8_t dev_addr, uint8_t reg_addr, uint8_t* p_buff,
                          uint16_t len) {
  return convertTypesST(HAL_I2C_Mem_Read(i2c_handle, dev_addr, reg_addr,
                                         I2C_MEMADD_SIZE_8BIT, p_buff, len,
                                         I2C_TIMEOUT));
}

status_t Peripheral::read(uint8_t dev_addr, uint8_t reg_addr, uint8_t& buff) {
  return convertTypesST(HAL_I2C_Mem_Read(i2c_handle, dev_addr, reg_addr,
                                         I2C_MEMADD_SIZE_8BIT, &buff, 1,
                                         I2C_TIMEOUT));
}

status_t Peripheral::write(uint8_t dev_addr, uint8_t reg_addr, uint8_t* p_buff,
                           uint16_t len) {
  return convertTypesST(HAL_I2C_Mem_Write(i2c_handle, dev_addr, reg_addr,
                                          I2C_MEMADD_SIZE_8BIT, p_buff, len,
                                          I2C_TIMEOUT));
}

status_t Peripheral::write(uint8_t dev_addr, uint8_t reg_addr, uint8_t& buff) {
  return convertTypesST(HAL_I2C_Mem_Write(i2c_handle, dev_addr, reg_addr,
                                          I2C_MEMADD_SIZE_8BIT, &buff, 1,
                                          I2C_TIMEOUT));
}

}  // namespace bio_sensors::i2c