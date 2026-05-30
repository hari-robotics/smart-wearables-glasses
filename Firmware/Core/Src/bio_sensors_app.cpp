

#include "bio_sensors/app/bio_sensors_app.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>

#include "bio_sensors/algo/ppg_processing.h"
#include "bio_sensors/bsp/peripherals.h"
#include "bio_sensors/bsp/system.h"
#include "bio_sensors/drivers/MAX30101.h"
#include "bio_sensors/drivers/MAX30101_config.h"
#include "bio_sensors/drivers/MAX30205.h"
#include "bio_sensors/drivers/MAX30205_config.h"
#include "bluetooth.h"


using namespace bio_sensors;

constexpr max30101_sample_rate_t PPG_SAMPLE_RATE = SPS100;

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

  bool updateVitals() {
    // PPG sensor data not ready, skip processing
    if (!ppg_interrupt_pending) {
      return false;
    }

    // Read newly arrived raw PPG samples from the driver FIFO.
    ppg_raw_data_t ppg_raw_data = {};
    uint16_t fetched_sample_count = 0U;
    ppg_status_t update_status =
        updateRawPpgData(&ppg_raw_data, &fetched_sample_count);
    ppg_interrupt_pending = false;

    if (update_status != ppg_status_t::PPG_OK || fetched_sample_count == 0U) {
      return false;
    }

    const algo::PpgVitalsProcessorOutput vitals_output =
        ppg_vitals_processor.Process(ppg_raw_data, fetched_sample_count);
    if (!vitals_output.has_skin_contact) {
      bio_sensors_data.heart_rate = 0.0f;
      bio_sensors_data.spo2_density = 0.0f;
      return true;
    }

    if (!vitals_output.vitals_ready) {
      return false;
    }

    // Keep the temperature reading the same rate as the vital data updates
    updateTemperature(bio_sensors_data.body_temperature);
    bio_sensors_data.heart_rate = vitals_output.heart_rate_bpm;
    bio_sensors_data.spo2_density = vitals_output.spo2_percent;
    return true;
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
  algo::PpgVitalsProcessor ppg_vitals_processor = {};

  // States
  bool ppg_sensor_ready = false;
  bool temperature_sensor_ready = false;
  bool ppg_interrupt_pending = false;

  ppg_status_t updateRawPpgData(ppg_raw_data_t* raw_data,
                                uint16_t* sample_count) {
    if (sample_count != nullptr) {
      *sample_count = 0U;
    }
    if (!ppg_sensor_ready || (ppg_sensor == nullptr) || (raw_data == nullptr)) {
      return ppg_status_t::PPG_ERROR;
    }

    ppg_status_t update_status = ppg_sensor->update();
    if (update_status != ppg_status_t::PPG_OK) {
      return update_status;
    }

    return ppg_sensor->getRawData(raw_data, sample_count);
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
  bool vitals_updated = sensors.updateVitals();

  // Push debugging data to BLE
  if (vitals_updated && (sys::time() - timestamp > 1000u)) {
    uint8_t ble_buffer[6] = {};
    sensors.serializeData(ble_buffer);
    BLE_SendPacket(DATA_TYPE_BIO_SENSORS, ble_buffer);
    timestamp = sys::time();
  }
}

void BioSensors_ExtiCpp() { sensors.setInterruptFlag(); }

void BioSensors_TimerCallbackCpp() {}
