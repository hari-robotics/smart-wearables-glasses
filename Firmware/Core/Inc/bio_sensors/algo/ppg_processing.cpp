#include "bio_sensors/algo/ppg_processing.h"

#include <cmath>

#include "bio_sensors/algo/ppg_math.h"
#include "bio_sensors/algo/ppg_vitals.h"

namespace bio_sensors::algo {

namespace {

constexpr float kHeartRateInitialConfirmDeltaBpm = 6.0f;
constexpr uint8_t kHeartRateInitialRequiredConfirmations = 2U;
constexpr float kHeartRateStableDeltaBpm = 3.0f;
constexpr float kHeartRateModerateDeltaBpm = 8.0f;
constexpr float kHeartRateConfirmDeltaBpm = 4.0f;
constexpr float kHeartRateMaxStepStableBpm = 2.0f;
constexpr float kHeartRateMaxStepModerateBpm = 3.5f;
constexpr float kHeartRateMaxStepConfirmedBpm = 5.0f;
constexpr uint8_t kHeartRateRequiredConfirmations = 2U;
// Calibrated from measured no-contact vs worn DC levels:
// no-contact red/ir ~= 1520/1780, worn red/ir ~= 80236/110626.
constexpr float kPpgContactMinRedDcLevel = 10000.0f;
constexpr float kPpgContactMinIrDcLevel = 12000.0f;
constexpr float kPpgContactMinPulsatility = 0.0008f;
constexpr float kPpgContactMinRedIrRatio = 0.10f;
constexpr float kPpgContactMaxRedIrRatio = 2.50f;

}  // namespace

PpgVitalsProcessorOutput PpgVitalsProcessor::Process(
    const ppg_raw_data_t& raw_data, uint16_t sample_count) {
  if (sample_count == 0U) {
    return {current_heart_rate_bpm, current_spo2_percent, false, false};
  }

  if (!hasReliableSkinContact(raw_data, sample_count)) {
    Reset();
    return {0.0f, 0.0f, false, false};
  }

  appendFilteredIrDataToWindow(raw_data.ir, sample_count);
  appendRawPpgDataToWindow(raw_data, sample_count);
  if (ppg_analysis_window_count < kSamplesRequiredForVitals) {
    return {current_heart_rate_bpm, current_spo2_percent, true, false};
  }

  const float estimated_heart_rate =
      EstimateHeartRateBpm(ppg_filtered_ir_window, ppg_analysis_window_count,
                           kEquivalentRateHz);
  if (estimated_heart_rate > 0.0f) {
    updateSmoothedHeartRate(estimated_heart_rate);
  }

  const float estimated_spo2 = EstimateSpo2Percent(
      ppg_analysis_window.red, ppg_analysis_window.ir,
      ppg_analysis_window_count);
  if (estimated_spo2 > 0.0f) {
    current_spo2_percent = estimated_spo2;
  }

  return {current_heart_rate_bpm, current_spo2_percent, true, true};
}

void PpgVitalsProcessor::Reset() {
  ppg_analysis_window = {};
  for (uint16_t index = 0U; index < kSamplesRequiredForVitals; ++index) {
    ppg_filtered_ir_window[index] = 0.0f;
  }
  ppg_analysis_window_count = 0U;
  ppg_ir_filter = {};
  ppg_ir_filter_ready = false;
  current_heart_rate_bpm = 0.0f;
  current_spo2_percent = 0.0f;
  pending_heart_rate_bpm = 0.0f;
  pending_heart_rate_confirmations = 0U;
}

bool PpgVitalsProcessor::hasReliableSkinContact(const ppg_raw_data_t& raw_data,
                                                uint16_t sample_count) const {
  if (sample_count < 4U) {
    return false;
  }

  SignalStats red_stats = {};
  SignalStats ir_stats = {};
  if (!ComputeSignalStats(raw_data.red, sample_count, &red_stats) ||
      !ComputeSignalStats(raw_data.ir, sample_count, &ir_stats)) {
    return false;
  }

  if ((red_stats.mean_sample < kPpgContactMinRedDcLevel) ||
      (ir_stats.mean_sample < kPpgContactMinIrDcLevel)) {
    return false;
  }

  const float red_ir_ratio = red_stats.mean_sample / ir_stats.mean_sample;
  if ((red_ir_ratio < kPpgContactMinRedIrRatio) ||
      (red_ir_ratio > kPpgContactMaxRedIrRatio)) {
    return false;
  }

  const float red_pulsatility = red_stats.ac_amplitude / red_stats.mean_sample;
  const float ir_pulsatility = ir_stats.ac_amplitude / ir_stats.mean_sample;
  if ((red_pulsatility < kPpgContactMinPulsatility) &&
      (ir_pulsatility < kPpgContactMinPulsatility)) {
    return false;
  }

  return true;
}

void PpgVitalsProcessor::appendRawPpgDataToWindow(const ppg_raw_data_t& raw_data,
                                                  uint16_t sample_count) {
  if (sample_count == 0U) {
    return;
  }

  if (sample_count >= kSamplesRequiredForVitals) {
    const uint16_t start_index = sample_count - kSamplesRequiredForVitals;
    for (uint16_t index = 0U; index < kSamplesRequiredForVitals; ++index) {
      ppg_analysis_window.ir[index] = raw_data.ir[start_index + index];
      ppg_analysis_window.red[index] = raw_data.red[start_index + index];
    }
    ppg_analysis_window_count = kSamplesRequiredForVitals;
    return;
  }

  uint16_t overflow_count = 0U;
  if ((ppg_analysis_window_count + sample_count) > kSamplesRequiredForVitals) {
    overflow_count =
        (ppg_analysis_window_count + sample_count) - kSamplesRequiredForVitals;
  }

  if (overflow_count > 0U) {
    const uint16_t remaining_count = ppg_analysis_window_count - overflow_count;
    for (uint16_t index = 0U; index < remaining_count; ++index) {
      ppg_analysis_window.ir[index] =
          ppg_analysis_window.ir[index + overflow_count];
      ppg_analysis_window.red[index] =
          ppg_analysis_window.red[index + overflow_count];
    }
    ppg_analysis_window_count = remaining_count;
  }

  for (uint16_t index = 0U; index < sample_count; ++index) {
    const uint16_t dst_index = ppg_analysis_window_count + index;
    ppg_analysis_window.ir[dst_index] = raw_data.ir[index];
    ppg_analysis_window.red[dst_index] = raw_data.red[index];
  }

  ppg_analysis_window_count += sample_count;
}

void PpgVitalsProcessor::appendFilteredIrDataToWindow(const uint32_t* ir_samples,
                                                      uint16_t sample_count) {
  if ((ir_samples == nullptr) || (sample_count == 0U)) {
    return;
  }

  float filtered_samples[MAX30101_BUFFER_SIZE] = {};
  for (uint16_t index = 0U; index < sample_count; ++index) {
    const float ir_sample = static_cast<float>(ir_samples[index]);
    if (!ppg_ir_filter_ready) {
      ButterworthBandPassFilter_Init(&ppg_ir_filter, ir_sample);
      ppg_ir_filter_ready = true;
    }

    filtered_samples[index] =
        ButterworthBandPassFilter_Process(&ppg_ir_filter, ir_sample);
  }

  appendFilteredIrBatchToWindow(filtered_samples, sample_count);
}

void PpgVitalsProcessor::appendFilteredIrBatchToWindow(
    const float* filtered_samples, uint16_t sample_count) {
  if ((filtered_samples == nullptr) || (sample_count == 0U)) {
    return;
  }

  if (sample_count >= kSamplesRequiredForVitals) {
    const uint16_t start_index = sample_count - kSamplesRequiredForVitals;
    for (uint16_t index = 0U; index < kSamplesRequiredForVitals; ++index) {
      ppg_filtered_ir_window[index] = filtered_samples[start_index + index];
    }
    return;
  }

  uint16_t overflow_count = 0U;
  if ((ppg_analysis_window_count + sample_count) > kSamplesRequiredForVitals) {
    overflow_count =
        (ppg_analysis_window_count + sample_count) - kSamplesRequiredForVitals;
  }

  if (overflow_count > 0U) {
    const uint16_t remaining_count = ppg_analysis_window_count - overflow_count;
    for (uint16_t index = 0U; index < remaining_count; ++index) {
      ppg_filtered_ir_window[index] =
          ppg_filtered_ir_window[index + overflow_count];
    }
  }

  const uint16_t dst_offset =
      (ppg_analysis_window_count > overflow_count)
          ? (ppg_analysis_window_count - overflow_count)
          : 0U;
  for (uint16_t index = 0U; index < sample_count; ++index) {
    ppg_filtered_ir_window[dst_offset + index] = filtered_samples[index];
  }
}

void PpgVitalsProcessor::updateSmoothedHeartRate(float estimated_heart_rate) {
  if (estimated_heart_rate <= 0.0f) {
    return;
  }

  if (current_heart_rate_bpm <= 0.0f) {
    if ((pending_heart_rate_confirmations == 0U) ||
        (std::fabs(estimated_heart_rate - pending_heart_rate_bpm) >
         kHeartRateInitialConfirmDeltaBpm)) {
      pending_heart_rate_bpm = estimated_heart_rate;
      pending_heart_rate_confirmations = 1U;
      return;
    }

    pending_heart_rate_bpm =
        0.5f * (pending_heart_rate_bpm + estimated_heart_rate);
    ++pending_heart_rate_confirmations;
    if (pending_heart_rate_confirmations <
        kHeartRateInitialRequiredConfirmations) {
      return;
    }

    current_heart_rate_bpm = pending_heart_rate_bpm;
    pending_heart_rate_bpm = 0.0f;
    pending_heart_rate_confirmations = 0U;
    return;
  }

  const float delta_bpm =
      std::fabs(estimated_heart_rate - current_heart_rate_bpm);
  if (delta_bpm <= kHeartRateStableDeltaBpm) {
    current_heart_rate_bpm = blendHeartRateTowards(
        current_heart_rate_bpm, estimated_heart_rate,
        kHeartRateMaxStepStableBpm);
    pending_heart_rate_bpm = 0.0f;
    pending_heart_rate_confirmations = 0U;
    return;
  }

  if (delta_bpm <= kHeartRateModerateDeltaBpm) {
    current_heart_rate_bpm = blendHeartRateTowards(
        current_heart_rate_bpm, estimated_heart_rate,
        kHeartRateMaxStepModerateBpm);
    pending_heart_rate_bpm = 0.0f;
    pending_heart_rate_confirmations = 0U;
    return;
  }

  if ((pending_heart_rate_confirmations == 0U) ||
      (std::fabs(estimated_heart_rate - pending_heart_rate_bpm) >
       kHeartRateConfirmDeltaBpm)) {
    pending_heart_rate_bpm = estimated_heart_rate;
    pending_heart_rate_confirmations = 1U;
    return;
  }

  ++pending_heart_rate_confirmations;
  if (pending_heart_rate_confirmations < kHeartRateRequiredConfirmations) {
    return;
  }

  current_heart_rate_bpm = blendHeartRateTowards(
      current_heart_rate_bpm, pending_heart_rate_bpm,
      kHeartRateMaxStepConfirmedBpm);
  pending_heart_rate_bpm = 0.0f;
  pending_heart_rate_confirmations = 0U;
}

float PpgVitalsProcessor::blendHeartRateTowards(float current_heart_rate,
                                                float target_heart_rate,
                                                float max_step_bpm) const {
  const float delta_bpm = target_heart_rate - current_heart_rate;
  if (delta_bpm > max_step_bpm) {
    return current_heart_rate + max_step_bpm;
  }
  if (delta_bpm < -max_step_bpm) {
    return current_heart_rate - max_step_bpm;
  }

  return target_heart_rate;
}

}  // namespace bio_sensors::algo
