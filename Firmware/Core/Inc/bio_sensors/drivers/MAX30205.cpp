#include "MAX30205.h"

#include "MAX30205_reg.h"
#include "bio_sensors/bsp/i2c_driver.h"

namespace bio_sensors {

// Static variable
MAX30205::MAX30205(i2c::Peripheral& bus)
    : i2c_bus(bus),
      MAX30205_I2C_ADDR(getDeviceAddress(CONN_GND, CONN_GND, CONN_GND)) {}

void MAX30205::init() {
  // Initialization code for MAX30205 can be added here if needed
  uint8_t config =
      MAX30205_CONFIG_CONTINUOUS | MAX30205_CONFIG_OS_COMPARATOR |
      MAX30205_CONFIG_OS_ACTIVE_LOW | MAX30205_CONFIG_FAULT_QUEUE_1 |
      MAX30205_CONFIG_FORMAT_NORMAL | MAX30205_CONFIG_TIMEOUT_DISABLE;
  writeConfig(config);
}

void MAX30205::readTemperature(float* temperature) {
  uint8_t data[2];
  i2c_bus.read(MAX30205_I2C_ADDR, MAX30205_REG_TEMPERATURE, data, 2);

  // Combine the two bytes to form the temperature value
  int16_t temp_raw = (data[0] << 8) | data[1];

  // Convert raw value to Celsius (assuming 16-bit signed integer)
  *temperature =
      temp_raw *
      MAX30205_TEMP_RESOLUTION_C_PER_LSB;  // MAX30205 has a resolution of
                                           // 0.00390625°C per LSB
}

void MAX30205::writeConfig(uint8_t config) {
  i2c_bus.write(MAX30205_I2C_ADDR, MAX30205_REG_CONFIGURATION, config);
}

uint8_t MAX30205::getDeviceAddress(max30205_pin_conn_t a2,
                                   max30205_pin_conn_t a1,
                                   max30205_pin_conn_t a0) {
  if ((a2 & CONN_GND)) {
    if ((a1 & CONN_GND) && (a0 & CONN_GND)) {
      return MAX30205_I2C_ADDR_WRITE_A2_GND_A1_GND_A0_GND;
    } else if ((a1 & CONN_GND) && (a0 & CONN_VDD)) {
      return MAX30205_I2C_ADDR_WRITE_A2_GND_A1_GND_A0_VDD;
    } else if ((a1 & CONN_GND) && (a0 & CONN_SCL)) {
      return MAX30205_I2C_ADDR_WRITE_A2_GND_A1_GND_A0_SCL;
    } else if ((a1 & CONN_GND) && (a0 & CONN_SDA)) {
      return MAX30205_I2C_ADDR_WRITE_A2_GND_A1_GND_A0_SDA;
    } else if ((a1 & CONN_VDD) && (a0 & CONN_GND)) {
      return MAX30205_I2C_ADDR_WRITE_A2_GND_A1_VDD_A0_GND;
    } else if ((a1 & CONN_VDD) && (a0 & CONN_VDD)) {
      return MAX30205_I2C_ADDR_WRITE_A2_GND_A1_VDD_A0_VDD;
    } else if ((a1 & CONN_VDD) && (a0 & CONN_SCL)) {
      return MAX30205_I2C_ADDR_WRITE_A2_GND_A1_VDD_A0_SCL;
    } else if ((a1 & CONN_VDD) && (a0 & CONN_SDA)) {
      return MAX30205_I2C_ADDR_WRITE_A2_GND_A1_VDD_A0_SDA;
    } else if ((a1 & CONN_SCL) && (a0 & CONN_GND)) {
      return MAX30205_I2C_ADDR_WRITE_A2_GND_A1_SCL_A0_GND;
    } else if ((a1 & CONN_SCL) && (a0 & CONN_VDD)) {
      return MAX30205_I2C_ADDR_WRITE_A2_GND_A1_SCL_A0_VDD;
    } else if ((a1 & CONN_SCL) && (a0 & CONN_SCL)) {
      return MAX30205_I2C_ADDR_WRITE_A2_GND_A1_SCL_A0_SCL;
    } else if ((a1 & CONN_SCL) && (a0 & CONN_SDA)) {
      return MAX30205_I2C_ADDR_WRITE_A2_GND_A1_SCL_A0_SDA;
    } else if ((a1 & CONN_SDA) && (a0 & CONN_GND)) {
      return MAX30205_I2C_ADDR_WRITE_A2_GND_A1_SDA_A0_GND;
    } else if ((a1 & CONN_SDA) && (a0 & CONN_VDD)) {
      return MAX30205_I2C_ADDR_WRITE_A2_GND_A1_SDA_A0_VDD;
    } else if ((a1 & CONN_SDA) && (a0 & CONN_SCL)) {
      return MAX30205_I2C_ADDR_WRITE_A2_GND_A1_SDA_A0_SCL;
    } else if ((a1 & CONN_SDA) && (a0 & CONN_SDA)) {
      return MAX30205_I2C_ADDR_WRITE_A2_GND_A1_SDA_A0_SDA;
    }
  } else if (a2 & CONN_VDD) {
    if ((a1 & CONN_GND) && (a0 & CONN_GND)) {
      return MAX30205_I2C_ADDR_WRITE_A2_VDD_A1_GND_A0_GND;
    } else if ((a1 & CONN_GND) && (a0 & CONN_VDD)) {
      return MAX30205_I2C_ADDR_WRITE_A2_VDD_A1_GND_A0_VDD;
    } else if ((a1 & CONN_GND) && (a0 & CONN_SCL)) {
      return MAX30205_I2C_ADDR_WRITE_A2_VDD_A1_GND_A0_SCL;
    } else if ((a1 & CONN_GND) && (a0 & CONN_SDA)) {
      return MAX30205_I2C_ADDR_WRITE_A2_VDD_A1_GND_A0_SDA;
    } else if ((a1 & CONN_VDD) && (a0 & CONN_GND)) {
      return MAX30205_I2C_ADDR_WRITE_A2_VDD_A1_VDD_A0_GND;
    } else if ((a1 & CONN_VDD) && (a0 & CONN_VDD)) {
      return MAX30205_I2C_ADDR_WRITE_A2_VDD_A1_VDD_A0_VDD;
    } else if ((a1 & CONN_VDD) && (a0 & CONN_SCL)) {
      return MAX30205_I2C_ADDR_WRITE_A2_VDD_A1_VDD_A0_SCL;
    } else if ((a1 & CONN_VDD) && (a0 & CONN_SDA)) {
      return MAX30205_I2C_ADDR_WRITE_A2_VDD_A1_VDD_A0_SDA;
    } else if ((a1 & CONN_SCL) && (a0 & CONN_GND)) {
      return MAX30205_I2C_ADDR_WRITE_A2_VDD_A1_SCL_A0_GND;
    } else if ((a1 & CONN_SCL) && (a0 & CONN_VDD)) {
      return MAX30205_I2C_ADDR_WRITE_A2_VDD_A1_SCL_A0_VDD;
    } else if ((a1 & CONN_SCL) && (a0 & CONN_SCL)) {
      return MAX30205_I2C_ADDR_WRITE_A2_VDD_A1_SCL_A0_SCL;
    } else if ((a1 & CONN_SCL) && (a0 & CONN_SDA)) {
      return MAX30205_I2C_ADDR_WRITE_A2_VDD_A1_SCL_A0_SDA;
    } else if ((a1 & CONN_SDA) && (a0 & CONN_GND)) {
      return MAX30205_I2C_ADDR_WRITE_A2_VDD_A1_SDA_A0_GND;
    } else if ((a1 & CONN_SDA) && (a0 & CONN_VDD)) {
      return MAX30205_I2C_ADDR_WRITE_A2_VDD_A1_SDA_A0_VDD;
    } else if ((a1 & CONN_SDA) && (a0 & CONN_SCL)) {
      return MAX30205_I2C_ADDR_WRITE_A2_VDD_A1_SDA_A0_SCL;
    } else if ((a1 & CONN_SDA) && (a0 & CONN_SDA)) {
      return MAX30205_I2C_ADDR_WRITE_A2_VDD_A1_SDA_A0_SDA;
    }
  }
  // Default address if no valid combination is found
  return MAX30205_I2C_DEFAULT_WRITE_ADDRESS;
}

}  // namespace bio_sensors