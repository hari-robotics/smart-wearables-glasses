#ifndef BIO_SENSORS_DRIVER_CONFIG_H
#define BIO_SENSORS_DRIVER_CONFIG_H

#include <stdint.h>

namespace bio_sensors {

class I2C_Interface {
public:
  void readReg(uint8_t *p_buff, uint8_t addr);
  void writeReg(uint8_t *p_buff, uint8_t addr);
};

}

#endif