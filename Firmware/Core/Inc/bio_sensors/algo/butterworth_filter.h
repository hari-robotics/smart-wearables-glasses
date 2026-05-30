#ifndef __BUTTERWORTH_FILTER_H__
#define __BUTTERWORTH_FILTER_H__

/**
 * \file butterworth_filter.h
 * \brief Third-order Butterworth band-pass filter used for IR PPG peak detection.
 */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The bundled coefficients are tuned for a 25 Hz effective PPG rate
 * (100 SPS sensor configuration with 4-sample averaging).
 */
#ifndef BIOSENSORS_PPG_EFFECTIVE_SAMPLE_RATE_HZ
#define BIOSENSORS_PPG_EFFECTIVE_SAMPLE_RATE_HZ 25U
#endif

typedef struct {
    float x1;
    float x2;
    float x3;
    float x4;
    float x5;
    float x6;
    float y1;
    float y2;
    float y3;
    float y4;
    float y5;
    float y6;
} ButterworthBandPassFilter;

void ButterworthBandPassFilter_Init(ButterworthBandPassFilter *filter, float initial_sample);
float ButterworthBandPassFilter_Process(ButterworthBandPassFilter *filter, float sample);

#ifdef __cplusplus
}
#endif

#endif /* __BUTTERWORTH_FILTER_H__ */
