#ifndef BIO_SENSORS_ALGO_PPG_MATH_H
#define BIO_SENSORS_ALGO_PPG_MATH_H

#include <cstddef>
#include <cstdint>

namespace bio_sensors::algo {

struct SignalStats {
  float min_sample;
  float max_sample;
  float mean_sample;
  float ac_amplitude;
};

float ClampFloat(float value, float min_value, float max_value);
bool ComputeSignalStats(const uint32_t* samples, std::size_t sample_count,
                        SignalStats* stats);
bool ComputeSignalStats(const float* samples, std::size_t sample_count,
                        SignalStats* stats);

}  // namespace bio_sensors::algo

#endif  // BIO_SENSORS_ALGO_PPG_MATH_H
