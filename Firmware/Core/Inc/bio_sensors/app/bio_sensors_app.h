#ifndef BIO_SENSORS_APP_BIO_SENSORS_APP_H
#define BIO_SENSORS_APP_BIO_SENSORS_APP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct {
  float heart_rate;
  float spo2_density;
  float body_temperature;
} BioSensorsData;

void BioSensors_InitCpp();
void BioSensors_LoopCpp();
void BioSensors_ExtiCpp();
void BioSensors_TimerCallbackCpp();

#ifdef __cplusplus
}
#endif

#endif