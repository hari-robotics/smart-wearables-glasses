#ifndef BIO_SENSORS_ALGO_PPG_VITALS_H
#define BIO_SENSORS_ALGO_PPG_VITALS_H

#include <cstddef>
#include <cstdint>

namespace bio_sensors::algo {

float EstimateHeartRateBpm(const float* samples, std::size_t sample_count,
                           uint32_t sample_rate_hz);
float EstimateSpo2Percent(const uint32_t* red_samples,
                          const uint32_t* ir_samples,
                          std::size_t sample_count);

}  // namespace bio_sensors::algo

#endif  // BIO_SENSORS_ALGO_PPG_VITALS_H
