#include "MAX30101.h"

#include <algorithm>
#include <cstdint>

#include "bio_sensors/bsp/delay.h"
#include "bio_sensors/bsp/i2c_driver.h"
#include "bio_sensors/drivers/MAX30101_reg.h"

namespace bio_sensors {

MAX30101::MAX30101(i2c::Peripheral& bus)
    : MAX30101_ADDR(MAX30101_I2C_ADDRESS << 1), i2c_bus(bus) {}

ppg_status_t MAX30101::init(const max30101_config_t* config) {
  if (config == nullptr) {
    return ppg_status_t::PPG_ERROR;
  }

  uint8_t part_id;
  readPartID(part_id);
  if (part_id != MAX30101_EXPECTED_PART_ID) {
    return ppg_status_t::PPG_ERROR;
  }
  if (reset() != ppg_status_t::PPG_OK) {
    return ppg_status_t::PPG_ERROR;
  };
  if (clearFifo() != ppg_status_t::PPG_OK) {
    return ppg_status_t::PPG_ERROR;
  };

  uint16_t dummy_value;
  (void)readIntStatus(dummy_value);

  if (writeConfig(config) != ppg_status_t::PPG_OK) {
    return ppg_status_t::PPG_ERROR;
  }
  return ppg_status_t::PPG_OK;
}

ppg_status_t MAX30101::readReg(uint8_t reg_addr, uint8_t& value) {
  if (i2c_bus.read(MAX30101_ADDR, reg_addr, value) == i2c::status_t::OK) {
    return ppg_status_t::PPG_OK;
  }
  return ppg_status_t::PPG_ERROR;
}

ppg_status_t MAX30101::writeReg(uint8_t reg_addr, uint8_t value) {
  if (i2c_bus.write(MAX30101_ADDR, reg_addr, value) == i2c::status_t::OK) {
    return ppg_status_t::PPG_OK;
  }
  return ppg_status_t::PPG_ERROR;
}

ppg_status_t MAX30101::readFifo(uint8_t* buffer, uint16_t len) {
  if (i2c_bus.read(MAX30101_ADDR, MAX30101_REG_FIFO_DATA, buffer, len) ==
      i2c::status_t::OK) {
    return ppg_status_t::PPG_OK;
  }
  return ppg_status_t::PPG_ERROR;
}

ppg_status_t MAX30101::readFifoPtr(uint8_t& p_write, uint8_t& p_read) {
  if (readReg(MAX30101_REG_FIFO_WR_PTR, p_write) != ppg_status_t::PPG_OK) {
    return ppg_status_t::PPG_ERROR;
  }
  if (readReg(MAX30101_REG_FIFO_RD_PTR, p_read) != ppg_status_t::PPG_OK) {
    return ppg_status_t::PPG_ERROR;
  }
  return PPG_OK;
}

ppg_status_t MAX30101::readIntStatus(uint16_t& int_status) {
  uint8_t int_status_u8array[2] = {};
  if (readReg(MAX30101_REG_INT_STATUS_1, int_status_u8array[0]) !=
      ppg_status_t::PPG_OK) {
    return ppg_status_t::PPG_ERROR;
  }
  if (readReg(MAX30101_REG_INT_STATUS_2, int_status_u8array[1]) !=
      ppg_status_t::PPG_OK) {
    return ppg_status_t::PPG_ERROR;
  }

  int_status = (static_cast<uint16_t>(int_status_u8array[1]) << 8) |
               static_cast<uint16_t>(int_status_u8array[0]);
  return ppg_status_t::PPG_OK;
}

ppg_status_t MAX30101::update() {
  uint16_t int_status;
  uint8_t p_write;
  uint8_t p_read;
  uint8_t samples_to_read;

  if (readIntStatus(int_status) != ppg_status_t::PPG_OK) {
    return ppg_status_t::PPG_ERROR;
  }

  if (readFifoPtr(p_write, p_read) != ppg_status_t::PPG_OK) {
    return ppg_status_t::PPG_ERROR;
  }

  if (p_write == p_read &&
      (int_status & (MAX30101_INT_A_FULL | MAX30101_INT_ALC_OVF))) {
    samples_to_read = MAX30101_FIFO_DEPTH_SAMPLES;
  } else {
    samples_to_read =
        (uint8_t)((p_write - p_read) & MAX30101_FIFO_POINTER_MASK);
  }

  bool updated = false;
  while (samples_to_read > 0) {
    uint8_t batch_samples = std::min(samples_to_read, MAX30101_BATCH_SIZE);
    uint16_t bytes_to_read =
        batch_samples * MAX30101_FIFO_BYTES_PER_SPO2_SAMPLE;

    // Fixed buffer to max length, because compiler does not support dynamic
    // array.
    uint8_t buffer[MAX30101_BATCH_SIZE * MAX30101_FIFO_BYTES_PER_SPO2_SAMPLE];

    if (readFifo(&buffer[0], bytes_to_read) != ppg_status_t::PPG_OK) {
      return ppg_status_t::PPG_ERROR;
    }

    for (uint8_t i = 0; i < batch_samples; i++) {
      uint8_t* sample_ptr = &buffer[i * MAX30101_FIFO_BYTES_PER_SPO2_SAMPLE];
      uint32_t red_sample =
          ((((uint32_t)sample_ptr[0]) << 16) |
           (((uint32_t)sample_ptr[1]) << 8) | ((uint32_t)sample_ptr[2])) &
          MAX30101_FIFO_DATA_MASK;
      uint32_t ir_sample =
          ((((uint32_t)sample_ptr[3]) << 16) |
           (((uint32_t)sample_ptr[4]) << 8) | ((uint32_t)sample_ptr[5])) &
          MAX30101_FIFO_DATA_MASK;
      pushBatch(ir_sample, red_sample);
    }
    samples_to_read -= batch_samples;
    updated = true;
  }
  if (updated) {
    return ppg_status_t::PPG_OK;
  }
  return ppg_status_t::PPG_NODATA;
}

void MAX30101::pushBatch(uint32_t ir_data, uint32_t red_data) {
  raw.ir[batch_index] = ir_data;
  raw.red[batch_index] = red_data;

  batch_index = (batch_index + 1) % MAX30101_BUFFER_SIZE;
}

ppg_status_t MAX30101::readPartID(uint8_t& part_id) {
  if (readReg(MAX30101_REG_PART_ID, part_id) != ppg_status_t::PPG_OK) {
    return ppg_status_t::PPG_ERROR;
  }
  return ppg_status_t::PPG_OK;
}

ppg_status_t MAX30101::reset() {
  if (writeReg(MAX30101_REG_MODE_CONFIG, MAX30101_MODE_RESET) !=
      ppg_status_t::PPG_OK) {
    return ppg_status_t::PPG_ERROR;
  }
  sys::delay(10);

  uint8_t default_config;
  for (uint8_t attempt = 0U; attempt < 10U; ++attempt) {
    if (readReg(MAX30101_REG_MODE_CONFIG, default_config) !=
        ppg_status_t::PPG_OK) {
      return ppg_status_t::PPG_ERROR;
    }
    if ((default_config & MAX30101_MODE_RESET) == 0U) {
      return ppg_status_t::PPG_OK;
    }
    sys::delay(2);
  }
  return ppg_status_t::PPG_ERROR;
}

ppg_status_t MAX30101::clearFifo() {
  if (writeReg(MAX30101_REG_FIFO_WR_PTR, 0u) != ppg_status_t::PPG_OK) {
    return ppg_status_t::PPG_ERROR;
  }
  if (writeReg(MAX30101_REG_OVF_COUNTER, 0u) != ppg_status_t::PPG_OK) {
    return ppg_status_t::PPG_ERROR;
  }
  if (writeReg(MAX30101_REG_FIFO_RD_PTR, 0u) != ppg_status_t::PPG_OK) {
    return ppg_status_t::PPG_ERROR;
  }
  return ppg_status_t::PPG_OK;
}

ppg_status_t MAX30101::writeConfig(const max30101_config_t* config) {
  if (writeReg(MAX30101_REG_FIFO_CONFIG, config->fifo_config) !=
      ppg_status_t::PPG_OK) {
    return ppg_status_t::PPG_ERROR;
  }
  if (writeReg(MAX30101_REG_MODE_CONFIG, config->mode) !=
      ppg_status_t::PPG_OK) {
    return ppg_status_t::PPG_ERROR;
  }
  if (writeReg(MAX30101_REG_SPO2_CONFIG, config->spo2_config) !=
      ppg_status_t::PPG_OK) {
    return ppg_status_t::PPG_ERROR;
  }
  if (writeReg(MAX30101_REG_LED1_PA, config->red_led_current_pa1) !=
      ppg_status_t::PPG_OK) {
    return ppg_status_t::PPG_ERROR;
  }
  if (writeReg(MAX30101_REG_LED2_PA, config->ir_led_current_pa2) !=
      ppg_status_t::PPG_OK) {
    return ppg_status_t::PPG_ERROR;
  }
  if (writeReg(MAX30101_REG_INT_ENABLE_1, config->int_enable_1) !=
      ppg_status_t::PPG_OK) {
    return ppg_status_t::PPG_ERROR;
  }
  if (writeReg(MAX30101_REG_INT_ENABLE_2, config->int_enable_2) !=
      ppg_status_t::PPG_OK) {
    return ppg_status_t::PPG_ERROR;
  }
  return ppg_status_t::PPG_OK;
}

void MAX30101::getRawData(ppg_raw_data_t* raw_data) {
  if (raw_data == nullptr) {
    return;
  }

  *raw_data = raw;
}

}  // namespace bio_sensors