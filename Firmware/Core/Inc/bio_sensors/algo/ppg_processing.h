#ifndef BIO_SENSORS_ALGO_PPG_PROCESSING_H
#define BIO_SENSORS_ALGO_PPG_PROCESSING_H

#include <cstdint>

#include "bio_sensors/algo/butterworth_filter.h"
#include "bio_sensors/drivers/MAX30101.h"

namespace bio_sensors::algo {

struct PpgVitalsProcessorOutput {
  float heart_rate_bpm;
  float spo2_percent;
  bool has_skin_contact;
  bool vitals_ready;
};

class PpgVitalsProcessor {
 public:
  PpgVitalsProcessor() = default;

  PpgVitalsProcessorOutput Process(const ppg_raw_data_t& raw_data,
                                   uint16_t sample_count);
  void Reset();

 private:
  static constexpr uint32_t kEquivalentRateHz =
      BIOSENSORS_PPG_EFFECTIVE_SAMPLE_RATE_HZ;
  static constexpr uint16_t kSamplesRequiredForVitals = 100U;

  ppg_raw_data_t ppg_analysis_window = {};
  float ppg_filtered_ir_window[kSamplesRequiredForVitals] = {};
  uint16_t ppg_analysis_window_count = 0U;
  ButterworthBandPassFilter ppg_ir_filter = {};
  bool ppg_ir_filter_ready = false;
  float current_heart_rate_bpm = 0.0f;
  float current_spo2_percent = 0.0f;
  float pending_heart_rate_bpm = 0.0f;
  uint8_t pending_heart_rate_confirmations = 0U;

  bool hasReliableSkinContact(const ppg_raw_data_t& raw_data,
                              uint16_t sample_count) const;
  void appendRawPpgDataToWindow(const ppg_raw_data_t& raw_data,
                                uint16_t sample_count);
  void appendFilteredIrDataToWindow(const uint32_t* ir_samples,
                                    uint16_t sample_count);
  void appendFilteredIrBatchToWindow(const float* filtered_samples,
                                     uint16_t sample_count);
  void updateSmoothedHeartRate(float estimated_heart_rate);
  float blendHeartRateTowards(float current_heart_rate,
                              float target_heart_rate,
                              float max_step_bpm) const;
};

}  // namespace bio_sensors::algo

#endif  // BIO_SENSORS_ALGO_PPG_PROCESSING_H
