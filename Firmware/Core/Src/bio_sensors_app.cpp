

#include "bio_sensors/app/bio_sensors_app.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>

#include "bio_sensors/algo/ppg_vitals.h"
#include "bio_sensors/bsp/peripherals.h"
#include "bio_sensors/bsp/system.h"
#include "bio_sensors/drivers/MAX30101.h"
#include "bio_sensors/drivers/MAX30101_config.h"
#include "bio_sensors/drivers/MAX30205.h"
#include "bio_sensors/drivers/MAX30205_config.h"
#include "bluetooth.h"

using namespace bio_sensors;

static BioSensorsData bio_sensors_data = {};
static bool ppg_sensor_ready = false;
static bool temperature_sensor_ready = false;
static volatile bool ppg_interrupt_pending = false;

std::unique_ptr<MAX30101> ppg_sensor;
std::unique_ptr<MAX30205> temperature_sensor;

namespace {

constexpr uint32_t kPpgSampleRateHz = 100U;
constexpr max30101_sample_rate_t sample_rate_cfg = SPS100;
constexpr uint16_t kMinPpgSamplesForVitals = 100U;

const max30101_config_t kPpgConfig = {
    MAX30101_FIFO_SAMPLE_AVG_4 | MAX30101_FIFO_ROLLOVER_EN |
        MAX30101_FIFO_A_FULL_24_SAMPLES,
    MAX30101_MODE_SPO2,
    MAX30101_ADC_RANGE_4096NA | MAX30101_SAMPLE_RATE(sample_rate_cfg) |
        MAX30101_PULSE_WIDTH_411_US,
    MAX30101_LED_CURRENT_12_6MA,
    MAX30101_LED_CURRENT_12_6MA,
    MAX30101_INT_EN_A_FULL | MAX30101_INT_EN_ALC_OVF,
    0U,
    0U,
    0U,
};

void UpdateBodyTemperature(BioSensorsData* data) {
  if ((data == nullptr) || !temperature_sensor_ready ||
      (temperature_sensor == nullptr)) {
    return;
  }

  float measured_temperature = 0.0f;
  if (temperature_sensor->readTemperature(&measured_temperature) ==
      i2c::status_t::OK) {
    data->body_temperature = measured_temperature;
  }
}

void UpdatePpgVitals(BioSensorsData* data) {
  if ((data == nullptr) || !ppg_sensor_ready || (ppg_sensor == nullptr)) {
    return;
  }

  const ppg_status_t update_status = ppg_sensor->update();
  if ((update_status != ppg_status_t::PPG_OK) &&
      (update_status != ppg_status_t::PPG_NODATA)) {
    return;
  }

  ppg_raw_data_t raw_data = {};
  uint16_t valid_sample_count = 0U;
  if (ppg_sensor->getRawData(&raw_data, &valid_sample_count) !=
          ppg_status_t::PPG_OK ||
      valid_sample_count == 0U) {
    return;
  }

  const std::size_t latest_sample_index = valid_sample_count - 1U;
  data->ir_led_raw = raw_data.ir[latest_sample_index];
  data->red_led_raw = raw_data.red[latest_sample_index];

  if (valid_sample_count < kMinPpgSamplesForVitals) {
    return;
  }

  data->heart_rate = algo::EstimateHeartRateBpm(raw_data.ir, valid_sample_count,
                                                kPpgSampleRateHz);
  data->spo2_density =
      algo::EstimateSpo2Percent(raw_data.red, raw_data.ir, valid_sample_count);
}

}  // namespace
void BioSensors_InitCpp() {
  ppg_interrupt_pending = false;
  ppg_sensor_ready = false;
  temperature_sensor_ready = false;

  ppg_sensor = std::make_unique<MAX30101>(peripheral::i2c3);
  temperature_sensor = std::make_unique<MAX30205>(peripheral::i2c3);

  ppg_sensor_ready = (ppg_sensor->init(&kPpgConfig) == ppg_status_t::PPG_OK);
  temperature_sensor_ready =
      (temperature_sensor->init(
           MAX30205_CONFIG_CONTINUOUS | MAX30205_CONFIG_OS_COMPARATOR |
           MAX30205_CONFIG_OS_ACTIVE_LOW | MAX30205_CONFIG_FAULT_QUEUE_1 |
           MAX30205_CONFIG_FORMAT_NORMAL | MAX30205_CONFIG_TIMEOUT_DISABLE) ==
       i2c::status_t::OK);
}

static uint32_t timestamp = 0;
void BioSensors_LoopCpp() {
  if (ppg_interrupt_pending) {
    ppg_interrupt_pending = false;
    UpdatePpgVitals(&bio_sensors_data);
  }
  UpdateBodyTemperature(&bio_sensors_data);

  if (sys::time() - timestamp > 1000u) {
    uint8_t ble_buffer[6];
    int16_t hr_data = floor(bio_sensors_data.heart_rate * 10.f);
    int16_t spo2_data = floor(bio_sensors_data.spo2_density * 10.f);
    int16_t temperature_data = floor(bio_sensors_data.body_temperature * 10.0f);
    ble_buffer[0] = static_cast<uint8_t>(hr_data >> 8);
    ble_buffer[1] = static_cast<uint8_t>(hr_data);
    ble_buffer[2] = static_cast<uint8_t>(spo2_data >> 8);
    ble_buffer[3] = static_cast<uint8_t>(spo2_data);
    ble_buffer[4] = static_cast<uint8_t>(temperature_data >> 8);
    ble_buffer[5] = static_cast<uint8_t>(temperature_data);
    BLE_SendPacket(DATA_TYPE_BIO_SENSORS, ble_buffer);
    timestamp = sys::time();
  }
}

void BioSensors_ExtiCpp() {
  if (ppg_sensor_ready) {
    ppg_interrupt_pending = true;
  }
}

void BioSensors_AcquireData(BioSensorsData* data) {
  if (data == nullptr) {
    return;
  }
  *data = bio_sensors_data;
}
