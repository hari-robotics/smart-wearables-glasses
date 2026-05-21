#include "MAX30101.h"

#include <algorithm>
#include <cstdint>

#include "bio_sensors/bsp/i2c_driver.h"
#include "bio_sensors/bsp/system.h"
#include "bio_sensors/drivers/MAX30101_config.h"

namespace bio_sensors {

MAX30101::MAX30101(i2c::Peripheral& bus)
    : i2c_bus(bus),
      MAX30101_ADDR(MAX30101_I2C_ADDRESS << 1),
      batch_index(0U),
      raw_sample_count(0U),
      fifo_bytes_per_sample(0U),
      red_channel_index(INVALID_CHANNEL_INDEX),
      ir_channel_index(INVALID_CHANNEL_INDEX),
      raw{} {}

ppg_status_t MAX30101::init(const max30101_config_t* config) {
  if (config == nullptr) {
    return ppg_status_t::PPG_ERROR;
  }

  uint8_t resolved_fifo_bytes_per_sample = 0U;
  uint8_t resolved_red_channel_index = INVALID_CHANNEL_INDEX;
  uint8_t resolved_ir_channel_index = INVALID_CHANNEL_INDEX;
  if (resolveSampleLayout(config, resolved_fifo_bytes_per_sample,
                          resolved_red_channel_index,
                          resolved_ir_channel_index) != ppg_status_t::PPG_OK) {
    return ppg_status_t::PPG_ERROR;
  }

  uint8_t part_id = 0U;
  if (readPartID(part_id) != ppg_status_t::PPG_OK) {
    return ppg_status_t::PPG_ERROR;
  }
  if (part_id != MAX30101_EXPECTED_PART_ID) {
    return ppg_status_t::PPG_ERROR;
  }
  if (reset() != ppg_status_t::PPG_OK) {
    return ppg_status_t::PPG_ERROR;
  }
  if (clearFifo() != ppg_status_t::PPG_OK) {
    return ppg_status_t::PPG_ERROR;
  }

  uint16_t dummy_value = 0U;
  (void)readIntStatus(dummy_value);

  if (writeConfig(config) != ppg_status_t::PPG_OK) {
    return ppg_status_t::PPG_ERROR;
  }

  fifo_bytes_per_sample = resolved_fifo_bytes_per_sample;
  red_channel_index = resolved_red_channel_index;
  ir_channel_index = resolved_ir_channel_index;

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
  if (fifo_bytes_per_sample == 0U) {
    return ppg_status_t::PPG_ERROR;
  }

  uint16_t int_status = 0U;
  uint8_t p_write = 0U;
  uint8_t p_read = 0U;
  uint8_t samples_to_read = 0U;

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
    uint16_t bytes_to_read = batch_samples * fifo_bytes_per_sample;

    // Size for the largest supported sample layout (4 slots x 3 bytes).
    uint8_t
        buffer[MAX30101_BATCH_SIZE * MAX30101_FIFO_BYTES_PER_MAX_MULTI_SAMPLE];

    if (readFifo(&buffer[0], bytes_to_read) != ppg_status_t::PPG_OK) {
      return ppg_status_t::PPG_ERROR;
    }

    for (uint8_t i = 0; i < batch_samples; i++) {
      uint8_t* sample_ptr = &buffer[i * fifo_bytes_per_sample];
      uint32_t red_sample = 0U;
      uint32_t ir_sample = 0U;

      if (red_channel_index != INVALID_CHANNEL_INDEX) {
        red_sample = decodeChannelSample(
            &sample_ptr[red_channel_index * MAX30101_FIFO_BYTES_PER_CHANNEL]);
      }

      if (ir_channel_index != INVALID_CHANNEL_INDEX) {
        ir_sample = decodeChannelSample(
            &sample_ptr[ir_channel_index * MAX30101_FIFO_BYTES_PER_CHANNEL]);
      }

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
  if (raw_sample_count < MAX30101_BUFFER_SIZE) {
    ++raw_sample_count;
  }
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
  resetRawData();
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
  if (writeReg(MAX30101_REG_MULTI_LED_CTRL1, config->multi_led_ctrl1) !=
      ppg_status_t::PPG_OK) {
    return ppg_status_t::PPG_ERROR;
  }
  if (writeReg(MAX30101_REG_MULTI_LED_CTRL2, config->multi_led_ctrl2) !=
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

ppg_status_t MAX30101::resolveSampleLayout(
    const max30101_config_t* config, uint8_t& fifo_bytes_per_sample_out,
    uint8_t& red_channel_index_out, uint8_t& ir_channel_index_out) const {
  const uint8_t mode = (uint8_t)(config->mode & MAX30101_MODE_MASK);

  fifo_bytes_per_sample_out = 0U;
  red_channel_index_out = INVALID_CHANNEL_INDEX;
  ir_channel_index_out = INVALID_CHANNEL_INDEX;

  switch (mode) {
    case MAX30101_MODE_HEART_RATE:
      fifo_bytes_per_sample_out = MAX30101_FIFO_BYTES_PER_HR_SAMPLE;
      red_channel_index_out = 0U;
      return ppg_status_t::PPG_OK;

    case MAX30101_MODE_SPO2:
      fifo_bytes_per_sample_out = MAX30101_FIFO_BYTES_PER_SPO2_SAMPLE;
      red_channel_index_out = 0U;
      ir_channel_index_out = 1U;
      return ppg_status_t::PPG_OK;

    case MAX30101_MODE_MULTI_LED: {
      const uint8_t slot_values[MAX30101_FIFO_MAX_ACTIVE_SLOTS] = {
          extractSlotValue(config->multi_led_ctrl1, MAX30101_SLOT1_SHIFT),
          extractSlotValue(config->multi_led_ctrl1, MAX30101_SLOT2_SHIFT),
          extractSlotValue(config->multi_led_ctrl2, MAX30101_SLOT3_SHIFT),
          extractSlotValue(config->multi_led_ctrl2, MAX30101_SLOT4_SHIFT),
      };
      uint8_t active_channels = 0U;

      for (uint8_t slot_value : slot_values) {
        if (slot_value == MAX30101_SLOT_DISABLED) {
          continue;
        }
        if (slot_value > MAX30101_SLOT_GREEN_LED) {
          return ppg_status_t::PPG_ERROR;
        }

        if (slot_value == MAX30101_SLOT_RED_LED) {
          if (red_channel_index_out != INVALID_CHANNEL_INDEX) {
            return ppg_status_t::PPG_ERROR;
          }
          red_channel_index_out = active_channels;
        } else if (slot_value == MAX30101_SLOT_IR_LED) {
          if (ir_channel_index_out != INVALID_CHANNEL_INDEX) {
            return ppg_status_t::PPG_ERROR;
          }
          ir_channel_index_out = active_channels;
        }

        ++active_channels;
      }

      if ((active_channels == 0U) ||
          ((red_channel_index_out == INVALID_CHANNEL_INDEX) &&
           (ir_channel_index_out == INVALID_CHANNEL_INDEX))) {
        return ppg_status_t::PPG_ERROR;
      }

      fifo_bytes_per_sample_out =
          active_channels * MAX30101_FIFO_BYTES_PER_CHANNEL;
      return ppg_status_t::PPG_OK;
    }

    default:
      return ppg_status_t::PPG_ERROR;
  }
}

uint8_t MAX30101::extractSlotValue(uint8_t slot_config, uint8_t shift) {
  return (uint8_t)((slot_config >> shift) & MAX30101_SLOT_MASK);
}

uint32_t MAX30101::decodeChannelSample(const uint8_t* channel_ptr) {
  return ((((uint32_t)channel_ptr[0]) << 16) |
          (((uint32_t)channel_ptr[1]) << 8) | ((uint32_t)channel_ptr[2])) &
         MAX30101_FIFO_DATA_MASK;
}

void MAX30101::resetRawData() {
  batch_index = 0U;
  raw_sample_count = 0U;
  raw = {};
}

ppg_status_t MAX30101::getRawData(ppg_raw_data_t* raw_data,
                                  uint16_t* sample_count) const {
  if (raw_data == nullptr) {
    return ppg_status_t::PPG_ERROR;
  }

  std::fill_n(raw_data->ir, MAX30101_BUFFER_SIZE, 0U);
  std::fill_n(raw_data->red, MAX30101_BUFFER_SIZE, 0U);

  const uint16_t exported_sample_count = raw_sample_count;
  if (sample_count != nullptr) {
    *sample_count = exported_sample_count;
  }

  if (exported_sample_count == 0U) {
    return ppg_status_t::PPG_OK;
  }

  uint16_t oldest_index = 0U;
  if (exported_sample_count == MAX30101_BUFFER_SIZE) {
    oldest_index = batch_index;
  }

  for (uint16_t sample_index = 0U; sample_index < exported_sample_count;
       ++sample_index) {
    const uint16_t raw_index =
        (uint16_t)((oldest_index + sample_index) % MAX30101_BUFFER_SIZE);
    raw_data->ir[sample_index] = raw.ir[raw_index];
    raw_data->red[sample_index] = raw.red[raw_index];
  }

  return ppg_status_t::PPG_OK;
}

}  // namespace bio_sensors
