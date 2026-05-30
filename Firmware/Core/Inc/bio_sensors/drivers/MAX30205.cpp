#include "MAX30205.h"

#include <cstdint>

#include "MAX30205_config.h"
#include "MAX30205_convert.h"
#include "bio_sensors/bsp/i2c_driver.h"

namespace bio_sensors {

using namespace peripherals;

constexpr uint8_t kMax30205AddressTable[2][4][4] = {
    {
        {
            MAX30205_I2C_ADDR_WRITE_A2_GND_A1_GND_A0_GND,
            MAX30205_I2C_ADDR_WRITE_A2_GND_A1_GND_A0_VDD,
            MAX30205_I2C_ADDR_WRITE_A2_GND_A1_GND_A0_SCL,
            MAX30205_I2C_ADDR_WRITE_A2_GND_A1_GND_A0_SDA,
        },
        {
            MAX30205_I2C_ADDR_WRITE_A2_GND_A1_VDD_A0_GND,
            MAX30205_I2C_ADDR_WRITE_A2_GND_A1_VDD_A0_VDD,
            MAX30205_I2C_ADDR_WRITE_A2_GND_A1_VDD_A0_SCL,
            MAX30205_I2C_ADDR_WRITE_A2_GND_A1_VDD_A0_SDA,
        },
        {
            MAX30205_I2C_ADDR_WRITE_A2_GND_A1_SCL_A0_GND,
            MAX30205_I2C_ADDR_WRITE_A2_GND_A1_SCL_A0_VDD,
            MAX30205_I2C_ADDR_WRITE_A2_GND_A1_SCL_A0_SCL,
            MAX30205_I2C_ADDR_WRITE_A2_GND_A1_SCL_A0_SDA,
        },
        {
            MAX30205_I2C_ADDR_WRITE_A2_GND_A1_SDA_A0_GND,
            MAX30205_I2C_ADDR_WRITE_A2_GND_A1_SDA_A0_VDD,
            MAX30205_I2C_ADDR_WRITE_A2_GND_A1_SDA_A0_SCL,
            MAX30205_I2C_ADDR_WRITE_A2_GND_A1_SDA_A0_SDA,
        },
    },
    {
        {
            MAX30205_I2C_ADDR_WRITE_A2_VDD_A1_GND_A0_GND,
            MAX30205_I2C_ADDR_WRITE_A2_VDD_A1_GND_A0_VDD,
            MAX30205_I2C_ADDR_WRITE_A2_VDD_A1_GND_A0_SCL,
            MAX30205_I2C_ADDR_WRITE_A2_VDD_A1_GND_A0_SDA,
        },
        {
            MAX30205_I2C_ADDR_WRITE_A2_VDD_A1_VDD_A0_GND,
            MAX30205_I2C_ADDR_WRITE_A2_VDD_A1_VDD_A0_VDD,
            MAX30205_I2C_ADDR_WRITE_A2_VDD_A1_VDD_A0_SCL,
            MAX30205_I2C_ADDR_WRITE_A2_VDD_A1_VDD_A0_SDA,
        },
        {
            MAX30205_I2C_ADDR_WRITE_A2_VDD_A1_SCL_A0_GND,
            MAX30205_I2C_ADDR_WRITE_A2_VDD_A1_SCL_A0_VDD,
            MAX30205_I2C_ADDR_WRITE_A2_VDD_A1_SCL_A0_SCL,
            MAX30205_I2C_ADDR_WRITE_A2_VDD_A1_SCL_A0_SDA,
        },
        {
            MAX30205_I2C_ADDR_WRITE_A2_VDD_A1_SDA_A0_GND,
            MAX30205_I2C_ADDR_WRITE_A2_VDD_A1_SDA_A0_VDD,
            MAX30205_I2C_ADDR_WRITE_A2_VDD_A1_SDA_A0_SCL,
            MAX30205_I2C_ADDR_WRITE_A2_VDD_A1_SDA_A0_SDA,
        },
    },
};

MAX30205::MAX30205(I2C& bus)
    : i2c_bus(bus),
      MAX30205_I2C_ADDR(getDeviceAddress(CONN_GND, CONN_GND, CONN_GND)) {}

i2c_status_t MAX30205::init(uint8_t config) {
  const i2c_status_t write_status = writeConfig(config);
  uint8_t read_back = 0U;

  if (write_status != i2c_status_t::OK) {
    return write_status;
  }

  const i2c_status_t read_status =
      i2c_bus.read(MAX30205_I2C_ADDR, MAX30205_REG_CONFIGURATION, read_back);
  if (read_status != i2c_status_t::OK) {
    return read_status;
  }

  return (read_back == config) ? i2c_status_t::OK : i2c_status_t::ERROR;
}

i2c_status_t MAX30205::readTemperature(float* temperature) {
  if (temperature == nullptr) {
    return i2c_status_t::ERROR;
  }

  uint8_t data[MAX30205_TEMPERATURE_REGISTER_SIZE_BYTES] = {0U, 0U};
  const i2c_status_t status =
      i2c_bus.read(MAX30205_I2C_ADDR, MAX30205_REG_TEMPERATURE, data,
                   MAX30205_TEMPERATURE_REGISTER_SIZE_BYTES);
  if (status != i2c_status_t::OK) {
    return status;
  }

  *temperature = MAX30205_BytesToCelsius(data[0], data[1], 0U);
  return i2c_status_t::OK;
}

i2c_status_t MAX30205::writeConfig(uint8_t config) {
  return i2c_bus.write(MAX30205_I2C_ADDR, MAX30205_REG_CONFIGURATION, config);
}

uint8_t MAX30205::getDeviceAddress(max30205_pin_conn_t a2,
                                   max30205_pin_conn_t a1,
                                   max30205_pin_conn_t a0) {
  if ((a2 > CONN_VDD) || (a1 > CONN_SDA) || (a0 > CONN_SDA)) {
    return MAX30205_I2C_DEFAULT_WRITE_ADDRESS;
  }

  return kMax30205AddressTable[a2][a1][a0];
}

}  // namespace bio_sensors
