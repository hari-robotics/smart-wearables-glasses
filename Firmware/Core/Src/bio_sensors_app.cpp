

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
#include "bio_sensors/algo/butterworth_filter.h"
#include "bluetooth.h"


using namespace bio_sensors;

constexpr uint32_t PPG_EQUIVALENT_RATE = BIOSENSORS_PPG_EFFECTIVE_SAMPLE_RATE_HZ;
constexpr max30101_sample_rate_t PPG_SAMPLE_RATE = SPS100;
constexpr uint16_t PPG_SAMPLES_REQUIRED_FOR_VITALS = 100U;

const max30101_config_t ppgSensorConfig = {
    MAX30101_FIFO_SAMPLE_AVG_4 | MAX30101_FIFO_ROLLOVER_EN |
        MAX30101_FIFO_A_FULL_24_SAMPLES,
    MAX30101_MODE_SPO2,
    MAX30101_ADC_RANGE_4096NA | MAX30101_SAMPLE_RATE(PPG_SAMPLE_RATE) |
        MAX30101_PULSE_WIDTH_411_US,
    MAX30101_LED_CURRENT_6_2MA,
    MAX30101_LED_CURRENT_6_2MA,
    MAX30101_INT_EN_A_FULL | MAX30101_INT_EN_ALC_OVF,
    0U,
    0U,
    0U,
};

// Shutdown mode, need to set one-shot trigger when reading temperature
const uint8_t temperatureSensorConfig =
    MAX30205_CONFIG_SHUTDOWN | MAX30205_CONFIG_OS_COMPARATOR |
    MAX30205_CONFIG_OS_ACTIVE_LOW | MAX30205_CONFIG_FAULT_QUEUE_1 |
    MAX30205_CONFIG_FORMAT_NORMAL | MAX30205_CONFIG_TIMEOUT_DISABLE;


class BioSensors {
 public:
  BioSensors() {
    ppg_sensor_ready = false;
    temperature_sensor_ready = false;
    ppg_interrupt_pending = false;
  }

  void init() {
    // Sensor initialization
    ppg_sensor = std::make_unique<MAX30101>(peripherals::i2c3);
    temperature_sensor = std::make_unique<MAX30205>(peripherals::i2c3);
    ppg_sensor_ready = (ppg_sensor->init(&ppgSensorConfig) == ppg_status_t::PPG_OK);
    temperature_sensor_ready = (temperature_sensor->init(temperatureSensorConfig) == peripherals::i2c_status_t::OK);
  }

  void updateVitals() {
    // PPG sensor data not ready, skip processing
    if (!ppg_interrupt_pending) {
      return;
    }

    // Read raw PPG data
    ppg_raw_data_t ppg_raw_data = {};
    uint16_t valid_sample_count = 0U;
    updateRawPpgData(&ppg_raw_data, &valid_sample_count);
    ppg_interrupt_pending = false;

    if (valid_sample_count < PPG_SAMPLES_REQUIRED_FOR_VITALS) {
      return;
    }

    // Keep the temperature reading the same rate as the vital data updates
    updateTemperature(bio_sensors_data.body_temperature);

    // Filter the raw IR data
    float filtered_ir[MAX30101_BUFFER_SIZE] = {};
    ButterworthBandPassFilter ir_filter;
    ButterworthBandPassFilter_Init(&ir_filter, static_cast<float>(ppg_raw_data.ir[0]));

    for (uint16_t index = 0; index < valid_sample_count; ++index) {
      float ir_sample = static_cast<float>(ppg_raw_data.ir[index]);
      float filtered_sample = ButterworthBandPassFilter_Process(&ir_filter, ir_sample);

      filtered_ir[index] = filtered_sample;
    }

    // Estimate vitals
    bio_sensors_data.heart_rate = algo::EstimateHeartRateBpm(filtered_ir, valid_sample_count,
                                              PPG_EQUIVALENT_RATE);
    bio_sensors_data.spo2_density =
        algo::EstimateSpo2Percent(ppg_raw_data.red, ppg_raw_data.ir, valid_sample_count);
  }

  void serializeData(uint8_t* buffer) {
    int16_t hr_data = floor(bio_sensors_data.heart_rate * 10.f);
    int16_t spo2_data = floor(bio_sensors_data.spo2_density * 10.f);
    int16_t temperature_data = floor(bio_sensors_data.body_temperature * 10.0f);
    buffer[0] = static_cast<uint8_t>(hr_data >> 8);
    buffer[1] = static_cast<uint8_t>(hr_data);
    buffer[2] = static_cast<uint8_t>(spo2_data >> 8);
    buffer[3] = static_cast<uint8_t>(spo2_data);
    buffer[4] = static_cast<uint8_t>(temperature_data >> 8);
    buffer[5] = static_cast<uint8_t>(temperature_data);
  }

  void setInterruptFlag() {
    if (ppg_sensor_ready) {
      ppg_interrupt_pending = true;
    }
  }

 private:
  // Sensor instances
  std::unique_ptr<MAX30101> ppg_sensor;
  std::unique_ptr<MAX30205> temperature_sensor;

  // Data storage
  BioSensorsData bio_sensors_data = {};

  // States
  bool ppg_sensor_ready = false;
  bool temperature_sensor_ready = false;
  bool ppg_interrupt_pending = false;

  void updateRawPpgData(ppg_raw_data_t* raw_data, uint16_t* sample_count) {
    if (!ppg_sensor_ready || (ppg_sensor == nullptr)) {
      return;
    }
    if (ppg_sensor->update() == ppg_status_t::PPG_OK) {
      ppg_sensor->getRawData(raw_data, sample_count);
    }
  }

  void updateTemperature(float& temperature) {
    if (!temperature_sensor_ready || (temperature_sensor == nullptr)) {
      return;
    }

    // Set one-shot trigger to start temperature measurement in shutdown mode
    if (temperature_sensor->writeConfig(temperatureSensorConfig |
                                        MAX30205_CONFIG_ONE_SHOT_START) ==
        peripherals::i2c_status_t::OK) {
      temperature_sensor->readTemperature(&temperature);
    }
  }
};


static BioSensors sensors;

void BioSensors_InitCpp() {
  sensors.init();
}

static uint32_t timestamp = 0;
void BioSensors_LoopCpp() {
  // Update sensor data
  sensors.updateVitals();

  // Push debugging data to BLE
  if (sys::time() - timestamp > 1000u) {
    uint8_t ble_buffer[6] = {};
    sensors.serializeData(ble_buffer);
    BLE_SendPacket(DATA_TYPE_BIO_SENSORS, ble_buffer);
    timestamp = sys::time();
  }
}

void BioSensors_ExtiCpp() { sensors.setInterruptFlag(); }

void BioSensors_TimerCallbackCpp() {
    
}