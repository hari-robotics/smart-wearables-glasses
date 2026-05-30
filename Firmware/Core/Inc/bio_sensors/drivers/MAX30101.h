#ifndef BIO_SENSORS_DRIVERS_MAX30101_H
#define BIO_SENSORS_DRIVERS_MAX30101_H

#include <cstdint>

#include "bio_sensors/bsp/i2c_driver.h"
#include "bio_sensors/drivers/MAX30101_config.h"

namespace bio_sensors {

typedef enum { PPG_OK, PPG_NODATA, PPG_ERROR } ppg_status_t;

typedef struct {
  uint32_t ir[MAX30101_BUFFER_SIZE];
  uint32_t red[MAX30101_BUFFER_SIZE];
} ppg_raw_data_t;

class MAX30101 {
 public:
  MAX30101(peripherals::I2C& bus);
  ppg_status_t init(const max30101_config_t* config);
  ppg_status_t update();
  ppg_status_t getRawData(ppg_raw_data_t* raw_data,
                          uint16_t* sample_count = nullptr);

 private:
  peripherals::I2C i2c_bus;
  const uint8_t MAX30101_ADDR;
  uint16_t raw_head_index;
  uint16_t raw_tail_index;
  uint16_t raw_sample_count;
  uint8_t fifo_bytes_per_sample;
  uint8_t red_channel_index;
  uint8_t ir_channel_index;
  ppg_raw_data_t raw;

  ppg_status_t readReg(uint8_t reg_addr, uint8_t& value);
  ppg_status_t writeReg(uint8_t reg_addr, uint8_t value);

  ppg_status_t readPartID(uint8_t& part_id);
  ppg_status_t readFifo(uint8_t* buffer, uint16_t len);
  ppg_status_t readFifoPtr(uint8_t& p_write, uint8_t& p_read);
  ppg_status_t readIntStatus(uint16_t& int_status);
  void pushBatch(uint32_t ir_data, uint32_t red_data);

  ppg_status_t reset();
  ppg_status_t clearFifo();
  ppg_status_t writeConfig(const max30101_config_t* config);
  ppg_status_t resolveSampleLayout(const max30101_config_t* config,
                                   uint8_t& fifo_bytes_per_sample_out,
                                   uint8_t& red_channel_index_out,
                                   uint8_t& ir_channel_index_out) const;
  static uint8_t extractSlotValue(uint8_t slot_config, uint8_t shift);
  static uint32_t decodeChannelSample(const uint8_t* channel_ptr);
  void resetRawData();

  static constexpr uint8_t INVALID_CHANNEL_INDEX = 0xFFU;
};

}  // namespace bio_sensors

#endif  // BIO_SENSORS_DRIVERS_MAX30101_H
