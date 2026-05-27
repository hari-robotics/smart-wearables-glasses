#include "bio_sensors/algo/ppg_math.h"

namespace bio_sensors::algo {

namespace {

bool FinalizeSignalStats(float min_sample, float max_sample, double sample_sum,
                         std::size_t sample_count, SignalStats* stats) {
  if (stats == nullptr) {
    return false;
  }

  stats->min_sample = min_sample;
  stats->max_sample = max_sample;
  stats->mean_sample = (float)(sample_sum / (double)sample_count);
  stats->ac_amplitude = (max_sample - min_sample) * 0.5f;
  return true;
}

}  // namespace

float ClampFloat(float value, float min_value, float max_value) {
  if (value < min_value) {
    return min_value;
  }

  if (value > max_value) {
    return max_value;
  }

  return value;
}

bool ComputeSignalStats(const uint32_t* samples, std::size_t sample_count,
                        SignalStats* stats) {
  if ((samples == nullptr) || (stats == nullptr) || (sample_count == 0U)) {
    return false;
  }

  float min_sample = (float)samples[0];
  float max_sample = (float)samples[0];
  uint64_t sample_sum = 0U;

  for (std::size_t sample_index = 0; sample_index < sample_count;
       ++sample_index) {
    const uint32_t sample = samples[sample_index];
    const float sample_as_float = (float)sample;

    if (sample_as_float < min_sample) {
      min_sample = sample_as_float;
    }
    if (sample_as_float > max_sample) {
      max_sample = sample_as_float;
    }

    sample_sum += sample;
  }

  return FinalizeSignalStats(min_sample, max_sample, (double)sample_sum,
                             sample_count, stats);
}

bool ComputeSignalStats(const float* samples, std::size_t sample_count,
                        SignalStats* stats) {
  if ((samples == nullptr) || (stats == nullptr) || (sample_count == 0U)) {
    return false;
  }

  float min_sample = samples[0];
  float max_sample = samples[0];
  double sample_sum = 0.0;

  for (std::size_t sample_index = 0; sample_index < sample_count;
       ++sample_index) {
    const float sample = samples[sample_index];

    if (sample < min_sample) {
      min_sample = sample;
    }
    if (sample > max_sample) {
      max_sample = sample;
    }

    sample_sum += sample;
  }

  return FinalizeSignalStats(min_sample, max_sample, sample_sum, sample_count,
                             stats);
}

}  // namespace bio_sensors::algo
