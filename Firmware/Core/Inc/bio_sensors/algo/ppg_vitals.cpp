#include "bio_sensors/algo/ppg_vitals.h"

#include "bio_sensors/algo/ppg_math.h"

namespace bio_sensors::algo {

float EstimateHeartRateBpm(const uint32_t* samples, std::size_t sample_count,
                           uint32_t sample_rate_hz) {
  if ((samples == nullptr) || (sample_count < 3U) || (sample_rate_hz == 0U)) {
    return 0.0f;
  }

  SignalStats signal_stats = {};
  if (!ComputeSignalStats(samples, sample_count, &signal_stats)) {
    return 0.0f;
  }

  if (signal_stats.max_sample <= signal_stats.min_sample) {
    return 0.0f;
  }

  const float threshold =
      (float)signal_stats.min_sample +
      ((float)(signal_stats.max_sample - signal_stats.min_sample) * 0.60f);
  std::size_t min_peak_spacing = (std::size_t)(sample_rate_hz / 2U);
  if (min_peak_spacing == 0U) {
    min_peak_spacing = 1U;
  }

  bool has_previous_peak = false;
  std::size_t previous_peak_index = 0U;
  float interval_sum = 0.0f;
  uint32_t interval_count = 0U;

  for (std::size_t sample_index = 1U; sample_index + 1U < sample_count;
       ++sample_index) {
    const uint32_t current_sample = samples[sample_index];
    const bool is_peak = ((float)current_sample >= threshold) &&
                         (current_sample > samples[sample_index - 1U]) &&
                         (current_sample >= samples[sample_index + 1U]);

    if (!is_peak) {
      continue;
    }

    if (has_previous_peak &&
        ((sample_index - previous_peak_index) <= min_peak_spacing)) {
      continue;
    }

    if (has_previous_peak) {
      interval_sum += (float)(sample_index - previous_peak_index);
      ++interval_count;
    }

    previous_peak_index = sample_index;
    has_previous_peak = true;
  }

  if (interval_count == 0U) {
    return 0.0f;
  }

  const float average_interval_samples = interval_sum / (float)interval_count;
  return 60.0f * (float)sample_rate_hz / average_interval_samples;
}

float EstimateSpo2Percent(const uint32_t* red_samples,
                          const uint32_t* ir_samples,
                          std::size_t sample_count) {
  if ((red_samples == nullptr) || (ir_samples == nullptr) ||
      (sample_count == 0U)) {
    return 0.0f;
  }

  SignalStats red_stats = {};
  SignalStats ir_stats = {};
  if (!ComputeSignalStats(red_samples, sample_count, &red_stats) ||
      !ComputeSignalStats(ir_samples, sample_count, &ir_stats)) {
    return 0.0f;
  }

  if ((red_stats.mean_sample <= 0.0f) || (ir_stats.mean_sample <= 0.0f) ||
      (red_stats.ac_amplitude <= 0.0f) || (ir_stats.ac_amplitude <= 0.0f)) {
    return 0.0f;
  }

  const float ratio = (red_stats.ac_amplitude / red_stats.mean_sample) /
                      (ir_stats.ac_amplitude / ir_stats.mean_sample);
  return ClampFloat(110.0f - (25.0f * ratio), 70.0f, 100.0f);
}

}  // namespace bio_sensors::algo
