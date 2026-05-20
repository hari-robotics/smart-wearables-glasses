

#include "bio_sensors/app/bio_sensors_app.h"

#include <memory>

#include "bio_sensors/bsp/peripherals.h"
#include "bio_sensors/drivers/MAX30101.h"
#include "bio_sensors/drivers/MAX30205.h"

using namespace bio_sensors;

static BioSensorsData bio_sensors_data = {};

std::unique_ptr<MAX30101> ppg_sensor =
    std::make_unique<MAX30101>(peripheral::i2c3);
std::unique_ptr<MAX30205> temperature_sensor =
    std::make_unique<MAX30205>(peripheral::i2c3);

void BioSensors_InitCpp() { temperature_sensor->init(); }

void BioSensors_LoopCpp() {}

void BioSensors_ExtiCpp() {}

void BioSensors_AcquireData(BioSensorsData* data) {
  if (data == nullptr) {
    return;
  }
  *data = bio_sensors_data;
}