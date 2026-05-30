#include "bio_sensors/algo/ppg_vitals.h"

#include <algorithm>
#include <cmath>

#include "bio_sensors/algo/ppg_math.h"
#include "bio_sensors/drivers/MAX30101_config.h"

namespace bio_sensors::algo {

namespace {

constexpr float kMinHeartRateBpm = 40.0f;
constexpr float kMaxHeartRateBpm = 200.0f;
constexpr float kMinSpo2Percent = 70.0f;
constexpr float kMaxSpo2Percent = 100.0f;
constexpr float kMinHeartRateAutocorrelationScore = 0.40f;
constexpr float kMinWaveformCorrelation = 0.35f;
constexpr float kMinSpo2Ratio = 0.2f;
constexpr float kMaxSpo2Ratio = 1.8f;
constexpr std::size_t kMaxDetectedPeaks = 32U;

float ComputeRms(const float* samples, std::size_t sample_count) {
  if ((samples == nullptr) || (sample_count == 0U)) {
    return 0.0f;
  }

  double square_sum = 0.0;
  for (std::size_t sample_index = 0U; sample_index < sample_count;
       ++sample_index) {
    const double sample = (double)samples[sample_index];
    square_sum += sample * sample;
  }

  return (float)std::sqrt(square_sum / (double)sample_count);
}

float RefinePeakLocation(const float* samples, std::size_t sample_index,
                         std::size_t sample_count) {
  if ((samples == nullptr) || (sample_index == 0U) ||
      ((sample_index + 1U) >= sample_count)) {
    return (float)sample_index;
  }

  const float previous_sample = samples[sample_index - 1U];
  const float current_sample = samples[sample_index];
  const float next_sample = samples[sample_index + 1U];
  const float denominator =
      previous_sample - (2.0f * current_sample) + next_sample;
  if (std::fabs(denominator) < 1.0e-6f) {
    return (float)sample_index;
  }

  float offset = 0.5f * (previous_sample - next_sample) / denominator;
  offset = ClampFloat(offset, -0.5f, 0.5f);
  return (float)sample_index + offset;
}

float ComputeMedian(float* values, std::size_t value_count) {
  if ((values == nullptr) || (value_count == 0U)) {
    return 0.0f;
  }

  std::sort(values, values + value_count);
  const std::size_t mid_index = value_count / 2U;
  if ((value_count & 0x01U) != 0U) {
    return values[mid_index];
  }

  return 0.5f * (values[mid_index - 1U] + values[mid_index]);
}

bool PrepareDetrendedSignal(const uint32_t* samples, std::size_t sample_count,
                            float* detrended_samples, float* dc_level,
                            float* ac_rms) {
  if ((samples == nullptr) || (detrended_samples == nullptr) ||
      (dc_level == nullptr) || (ac_rms == nullptr) || (sample_count < 2U) ||
      (sample_count > MAX30101_BUFFER_SIZE)) {
    return false;
  }

  const float first_sample = (float)samples[0];
  const float last_sample = (float)samples[sample_count - 1U];
  const float slope =
      (last_sample - first_sample) / (float)(sample_count - 1U);

  double dc_sum = 0.0;
  double detrended_sum = 0.0;
  for (std::size_t sample_index = 0U; sample_index < sample_count;
       ++sample_index) {
    const float sample = (float)samples[sample_index];
    const float baseline = first_sample + (slope * (float)sample_index);
    const float detrended_sample = sample - baseline;

    detrended_samples[sample_index] = detrended_sample;
    dc_sum += (double)sample;
    detrended_sum += (double)detrended_sample;
  }

  const float detrended_mean =
      (float)(detrended_sum / (double)sample_count);
  double square_sum = 0.0;
  for (std::size_t sample_index = 0U; sample_index < sample_count;
       ++sample_index) {
    detrended_samples[sample_index] -= detrended_mean;
    const double centered_sample = (double)detrended_samples[sample_index];
    square_sum += centered_sample * centered_sample;
  }

  const float initial_rms =
      (float)std::sqrt(square_sum / (double)sample_count);
  if (initial_rms <= 0.0f) {
    return false;
  }

  const float clip_limit = initial_rms * 3.0f;
  double clipped_square_sum = 0.0;
  std::size_t kept_sample_count = 0U;
  for (std::size_t sample_index = 0U; sample_index < sample_count;
       ++sample_index) {
    const float sample = detrended_samples[sample_index];
    if (std::fabs(sample) > clip_limit) {
      detrended_samples[sample_index] = 0.0f;
      continue;
    }

    clipped_square_sum += (double)sample * (double)sample;
    ++kept_sample_count;
  }

  if (kept_sample_count == 0U) {
    return false;
  }

  *dc_level = (float)(dc_sum / (double)sample_count);
  *ac_rms = (float)std::sqrt(clipped_square_sum / (double)kept_sample_count);
  return (*dc_level > 0.0f) && (*ac_rms > 0.0f);
}

float ComputeWaveformCorrelation(const float* lhs_samples,
                                 const float* rhs_samples,
                                 std::size_t sample_count) {
  if ((lhs_samples == nullptr) || (rhs_samples == nullptr) ||
      (sample_count == 0U)) {
    return 0.0f;
  }

  double lhs_square_sum = 0.0;
  double rhs_square_sum = 0.0;
  double product_sum = 0.0;
  for (std::size_t sample_index = 0U; sample_index < sample_count;
       ++sample_index) {
    const double lhs_sample = (double)lhs_samples[sample_index];
    const double rhs_sample = (double)rhs_samples[sample_index];

    lhs_square_sum += lhs_sample * lhs_sample;
    rhs_square_sum += rhs_sample * rhs_sample;
    product_sum += lhs_sample * rhs_sample;
  }

  if ((lhs_square_sum <= 0.0) || (rhs_square_sum <= 0.0)) {
    return 0.0f;
  }

  return (float)(product_sum / std::sqrt(lhs_square_sum * rhs_square_sum));
}

float EstimateHeartRateFromAutocorrelation(const float* samples,
                                           std::size_t sample_count,
                                           uint32_t sample_rate_hz) {
  if ((samples == nullptr) || (sample_count < 4U) || (sample_rate_hz == 0U) ||
      (sample_count > MAX30101_BUFFER_SIZE)) {
    return 0.0f;
  }

  SignalStats signal_stats = {};
  if (!ComputeSignalStats(samples, sample_count, &signal_stats)) {
    return 0.0f;
  }

  float centered_samples[MAX30101_BUFFER_SIZE] = {};
  for (std::size_t sample_index = 0U; sample_index < sample_count;
       ++sample_index) {
    centered_samples[sample_index] = samples[sample_index] - signal_stats.mean_sample;
  }

  std::size_t min_lag = (std::size_t)std::lround(
      ((float)sample_rate_hz * 60.0f) / kMaxHeartRateBpm);
  std::size_t max_lag = (std::size_t)std::lround(
      ((float)sample_rate_hz * 60.0f) / kMinHeartRateBpm);
  min_lag = std::max<std::size_t>(1U, min_lag);
  max_lag = std::min<std::size_t>(sample_count - 2U, max_lag);
  if (max_lag <= min_lag) {
    return 0.0f;
  }

  float lag_scores[MAX30101_BUFFER_SIZE] = {};
  float best_score = -1.0f;
  std::size_t best_lag = 0U;
  for (std::size_t lag = min_lag; lag <= max_lag; ++lag) {
    double product_sum = 0.0;
    double lhs_square_sum = 0.0;
    double rhs_square_sum = 0.0;
    const std::size_t overlap = sample_count - lag;

    for (std::size_t sample_index = 0U; sample_index < overlap; ++sample_index) {
      const double lhs_sample = (double)centered_samples[sample_index];
      const double rhs_sample = (double)centered_samples[sample_index + lag];
      product_sum += lhs_sample * rhs_sample;
      lhs_square_sum += lhs_sample * lhs_sample;
      rhs_square_sum += rhs_sample * rhs_sample;
    }

    if ((lhs_square_sum <= 0.0) || (rhs_square_sum <= 0.0)) {
      continue;
    }

    const float score =
        (float)(product_sum / std::sqrt(lhs_square_sum * rhs_square_sum));
    lag_scores[lag] = score;
    if (score > best_score) {
      best_score = score;
      best_lag = lag;
    }
  }

  if ((best_lag == 0U) || (best_score < kMinHeartRateAutocorrelationScore)) {
    return 0.0f;
  }

  float refined_lag = (float)best_lag;
  if ((best_lag > min_lag) && (best_lag < max_lag)) {
    const float previous_score = lag_scores[best_lag - 1U];
    const float current_score = lag_scores[best_lag];
    const float next_score = lag_scores[best_lag + 1U];
    const float denominator =
        previous_score - (2.0f * current_score) + next_score;
    if (std::fabs(denominator) >= 1.0e-6f) {
      float offset = 0.5f * (previous_score - next_score) / denominator;
      offset = ClampFloat(offset, -0.5f, 0.5f);
      refined_lag += offset;
    }
  }

  if (refined_lag <= 0.0f) {
    return 0.0f;
  }

  const float heart_rate_bpm =
      60.0f * (float)sample_rate_hz / refined_lag;
  return ClampFloat(heart_rate_bpm, kMinHeartRateBpm, kMaxHeartRateBpm);
}

}  // namespace

float EstimateHeartRateBpm(const float* samples, std::size_t sample_count,
                           uint32_t sample_rate_hz) {
  if ((samples == nullptr) || (sample_count < 3U) || (sample_rate_hz == 0U) ||
      (sample_count > MAX30101_BUFFER_SIZE)) {
    return 0.0f;
  }

  SignalStats signal_stats = {};
  if (!ComputeSignalStats(samples, sample_count, &signal_stats)) {
    return 0.0f;
  }

  float peak_samples[MAX30101_BUFFER_SIZE] = {};
  const float positive_excursion = signal_stats.max_sample - signal_stats.mean_sample;
  const float negative_excursion = signal_stats.mean_sample - signal_stats.min_sample;
  const bool invert_signal = negative_excursion > (positive_excursion * 1.10f);
  const float* analysis_samples = samples;
  if (invert_signal) {
    for (std::size_t sample_index = 0U; sample_index < sample_count;
         ++sample_index) {
      peak_samples[sample_index] = -samples[sample_index];
    }
    analysis_samples = peak_samples;
    if (!ComputeSignalStats(analysis_samples, sample_count, &signal_stats)) {
      return EstimateHeartRateFromAutocorrelation(samples, sample_count,
                                                  sample_rate_hz);
    }
  }

  if (signal_stats.max_sample <= signal_stats.min_sample) {
    return EstimateHeartRateFromAutocorrelation(samples, sample_count,
                                                sample_rate_hz);
  }

  const float signal_rms = ComputeRms(analysis_samples, sample_count);
  const float dynamic_range = signal_stats.max_sample - signal_stats.min_sample;
  if ((dynamic_range <= 0.0f) || (signal_rms <= 0.0f)) {
    return EstimateHeartRateFromAutocorrelation(samples, sample_count,
                                                sample_rate_hz);
  }

  std::size_t min_peak_spacing = (std::size_t)std::lround(
      ((float)sample_rate_hz * 60.0f) / kMaxHeartRateBpm);
  std::size_t max_peak_spacing = (std::size_t)std::lround(
      ((float)sample_rate_hz * 60.0f) / kMinHeartRateBpm);
  if (min_peak_spacing == 0U) {
    min_peak_spacing = 1U;
  }
  if (max_peak_spacing <= min_peak_spacing) {
    max_peak_spacing = min_peak_spacing + 1U;
  }

  const std::size_t prominence_window =
      std::max<std::size_t>(2U, min_peak_spacing);
  const float peak_height_threshold =
      signal_stats.mean_sample + (dynamic_range * 0.10f);
  const float prominence_threshold =
      std::max(dynamic_range * 0.18f, signal_rms * 0.75f);

  float peak_positions[kMaxDetectedPeaks] = {};
  float peak_scores[kMaxDetectedPeaks] = {};
  std::size_t peak_count = 0U;

  for (std::size_t sample_index = 1U; sample_index + 1U < sample_count;
       ++sample_index) {
    const float current_sample = analysis_samples[sample_index];
    const bool is_peak =
        (current_sample > analysis_samples[sample_index - 1U]) &&
        (current_sample >= analysis_samples[sample_index + 1U]);
    if (!is_peak || (current_sample < peak_height_threshold)) {
      continue;
    }

    const std::size_t window_start =
        (sample_index > prominence_window) ? (sample_index - prominence_window)
                                           : 0U;
    const std::size_t window_end =
        std::min(sample_count - 1U, sample_index + prominence_window);

    float left_min = current_sample;
    for (std::size_t index = window_start; index < sample_index; ++index) {
      if (analysis_samples[index] < left_min) {
        left_min = analysis_samples[index];
      }
    }

    float right_min = current_sample;
    for (std::size_t index = sample_index + 1U; index <= window_end; ++index) {
      if (analysis_samples[index] < right_min) {
        right_min = analysis_samples[index];
      }
    }

    const float prominence =
        std::min(current_sample - left_min, current_sample - right_min);
    if (prominence < prominence_threshold) {
      continue;
    }

    const float refined_peak_position =
        RefinePeakLocation(analysis_samples, sample_index, sample_count);
    const float peak_score = prominence + (current_sample * 0.25f);

    if ((peak_count > 0U) &&
        ((refined_peak_position - peak_positions[peak_count - 1U]) <
         (float)min_peak_spacing)) {
      if (peak_score > peak_scores[peak_count - 1U]) {
        peak_positions[peak_count - 1U] = refined_peak_position;
        peak_scores[peak_count - 1U] = peak_score;
      }
      continue;
    }

    if (peak_count < kMaxDetectedPeaks) {
      peak_positions[peak_count] = refined_peak_position;
      peak_scores[peak_count] = peak_score;
      ++peak_count;
    }
  }

  if (peak_count < 2U) {
    return EstimateHeartRateFromAutocorrelation(samples, sample_count,
                                                sample_rate_hz);
  }

  float intervals[kMaxDetectedPeaks] = {};
  std::size_t interval_count = 0U;
  for (std::size_t peak_index = 1U; peak_index < peak_count; ++peak_index) {
    const float interval =
        peak_positions[peak_index] - peak_positions[peak_index - 1U];
    if ((interval < (float)min_peak_spacing) ||
        (interval > (float)max_peak_spacing)) {
      continue;
    }

    intervals[interval_count] = interval;
    ++interval_count;
  }

  if (interval_count == 0U) {
    return EstimateHeartRateFromAutocorrelation(samples, sample_count,
                                                sample_rate_hz);
  }

  float sorted_intervals[kMaxDetectedPeaks] = {};
  for (std::size_t interval_index = 0U; interval_index < interval_count;
       ++interval_index) {
    sorted_intervals[interval_index] = intervals[interval_index];
  }

  const float median_interval =
      ComputeMedian(sorted_intervals, interval_count);
  if (median_interval <= 0.0f) {
    return EstimateHeartRateFromAutocorrelation(samples, sample_count,
                                                sample_rate_hz);
  }

  const float interval_tolerance =
      std::max(1.5f, median_interval * 0.20f);
  float interval_sum = 0.0f;
  std::size_t valid_interval_count = 0U;
  for (std::size_t interval_index = 0U; interval_index < interval_count;
       ++interval_index) {
    const float interval = intervals[interval_index];
    if (std::fabs(interval - median_interval) > interval_tolerance) {
      continue;
    }

    interval_sum += interval;
    ++valid_interval_count;
  }

  if (valid_interval_count == 0U) {
    return EstimateHeartRateFromAutocorrelation(samples, sample_count,
                                                sample_rate_hz);
  }

  const float average_interval_samples =
      interval_sum / (float)valid_interval_count;
  const float heart_rate_bpm =
      60.0f * (float)sample_rate_hz / average_interval_samples;
  return ClampFloat(heart_rate_bpm, kMinHeartRateBpm, kMaxHeartRateBpm);
}

float EstimateSpo2Percent(const uint32_t* red_samples,
                          const uint32_t* ir_samples,
                          std::size_t sample_count) {
  if ((red_samples == nullptr) || (ir_samples == nullptr) ||
      (sample_count < 2U) || (sample_count > MAX30101_BUFFER_SIZE)) {
    return 0.0f;
  }

  float red_detrended_samples[MAX30101_BUFFER_SIZE] = {};
  float ir_detrended_samples[MAX30101_BUFFER_SIZE] = {};
  float red_dc_level = 0.0f;
  float ir_dc_level = 0.0f;
  float red_ac_rms = 0.0f;
  float ir_ac_rms = 0.0f;
  if (!PrepareDetrendedSignal(red_samples, sample_count, red_detrended_samples,
                              &red_dc_level, &red_ac_rms) ||
      !PrepareDetrendedSignal(ir_samples, sample_count, ir_detrended_samples,
                              &ir_dc_level, &ir_ac_rms)) {
    return 0.0f;
  }

  if ((red_dc_level <= 0.0f) || (ir_dc_level <= 0.0f) ||
      (red_ac_rms <= 0.0f) || (ir_ac_rms <= 0.0f)) {
    return 0.0f;
  }

  const float waveform_correlation = ComputeWaveformCorrelation(
      red_detrended_samples, ir_detrended_samples, sample_count);
  if (waveform_correlation < kMinWaveformCorrelation) {
    return 0.0f;
  }

  const float ratio = (red_ac_rms / red_dc_level) / (ir_ac_rms / ir_dc_level);
  if ((ratio < kMinSpo2Ratio) || (ratio > kMaxSpo2Ratio)) {
    return 0.0f;
  }

  return ClampFloat(110.0f - (25.0f * ratio), kMinSpo2Percent,
                    kMaxSpo2Percent);
}

}  // namespace bio_sensors::algo
