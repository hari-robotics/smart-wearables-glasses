#ifndef BIO_SENSORS_DRIVERS_MAX30205_H
#define BIO_SENSORS_DRIVERS_MAX30205_H

#include <cstdint>

#include "bio_sensors/bsp/i2c_driver.h"

namespace bio_sensors {

typedef enum { CONN_GND, CONN_VDD, CONN_SCL, CONN_SDA } max30205_pin_conn_t;

class MAX30205 {
 public:
  MAX30205(i2c::Peripheral& bus);
  i2c::status_t init(uint8_t config);
  i2c::status_t readTemperature(float* temperature);

 private:
  i2c::Peripheral i2c_bus;
  uint8_t MAX30205_I2C_ADDR;
  uint8_t getDeviceAddress(max30205_pin_conn_t a2, max30205_pin_conn_t a1,
                           max30205_pin_conn_t a0);
  i2c::status_t writeConfig(uint8_t config);
};

}  // namespace bio_sensors

#endif
