#include "bio_sensors/algo/ppg_math.h"

namespace bio_sensors::algo {

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

  uint32_t min_sample = samples[0];
  uint32_t max_sample = samples[0];
  uint64_t sample_sum = 0U;

  for (std::size_t sample_index = 0; sample_index < sample_count;
       ++sample_index) {
    const uint32_t sample = samples[sample_index];

    if (sample < min_sample) {
      min_sample = sample;
    }
    if (sample > max_sample) {
      max_sample = sample;
    }

    sample_sum += sample;
  }

  stats->min_sample = min_sample;
  stats->max_sample = max_sample;
  stats->mean_sample = (float)sample_sum / (float)sample_count;
  stats->ac_amplitude = (float)(max_sample - min_sample) * 0.5f;

  return true;
}

}  // namespace bio_sensors::algo
