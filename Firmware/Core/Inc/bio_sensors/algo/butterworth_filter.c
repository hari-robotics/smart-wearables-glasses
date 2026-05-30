#include "butterworth_filter.h"

/*
 * Passa banda Butterworth del terzo ordine per il segnale IR PPG.
 * Coefficienti calcolati per fs = 25 Hz e banda 0.5-4 Hz, cioe circa
 * 30-240 bpm. Il filtro digitale risultante e di ordine 6.
 */
#if (BIOSENSORS_PPG_EFFECTIVE_SAMPLE_RATE_HZ != 25U)
#warning "Aggiornare i coefficienti del filtro Butterworth IR per la nuova frequenza di campionamento."
#endif
#define BUTTERWORTH_IR_B0             0.041876828242f
#define BUTTERWORTH_IR_B1             0.0f
#define BUTTERWORTH_IR_B2            -0.125630484727f
#define BUTTERWORTH_IR_B3             0.0f
#define BUTTERWORTH_IR_B4             0.125630484727f
#define BUTTERWORTH_IR_B5             0.0f
#define BUTTERWORTH_IR_B6            -0.041876828242f
#define BUTTERWORTH_IR_A1            -3.994126021730f
#define BUTTERWORTH_IR_A2             6.797137435589f
#define BUTTERWORTH_IR_A3            -6.448407217307f
#define BUTTERWORTH_IR_A4             3.657125155260f
#define BUTTERWORTH_IR_A5            -1.170537398811f
#define BUTTERWORTH_IR_A6             0.159769122452f

void ButterworthBandPassFilter_Init(ButterworthBandPassFilter *filter, float initial_sample)
{
    if (filter == 0) {
        return;
    }

    filter->x1 = initial_sample;
    filter->x2 = initial_sample;
    filter->x3 = initial_sample;
    filter->x4 = initial_sample;
    filter->x5 = initial_sample;
    filter->x6 = initial_sample;
    filter->y1 = 0.0f;
    filter->y2 = 0.0f;
    filter->y3 = 0.0f;
    filter->y4 = 0.0f;
    filter->y5 = 0.0f;
    filter->y6 = 0.0f;
}

float ButterworthBandPassFilter_Process(ButterworthBandPassFilter *filter, float sample)
{
    float filtered_sample;

    if (filter == 0) {
        return 0.0f;
    }

    filtered_sample =
        (BUTTERWORTH_IR_B0 * sample) +
        (BUTTERWORTH_IR_B1 * filter->x1) +
        (BUTTERWORTH_IR_B2 * filter->x2) +
        (BUTTERWORTH_IR_B3 * filter->x3) +
        (BUTTERWORTH_IR_B4 * filter->x4) +
        (BUTTERWORTH_IR_B5 * filter->x5) +
        (BUTTERWORTH_IR_B6 * filter->x6) -
        (BUTTERWORTH_IR_A1 * filter->y1) -
        (BUTTERWORTH_IR_A2 * filter->y2) -
        (BUTTERWORTH_IR_A3 * filter->y3) -
        (BUTTERWORTH_IR_A4 * filter->y4) -
        (BUTTERWORTH_IR_A5 * filter->y5) -
        (BUTTERWORTH_IR_A6 * filter->y6);

    filter->x6 = filter->x5;
    filter->x5 = filter->x4;
    filter->x4 = filter->x3;
    filter->x3 = filter->x2;
    filter->x2 = filter->x1;
    filter->x1 = sample;
    filter->y6 = filter->y5;
    filter->y5 = filter->y4;
    filter->y4 = filter->y3;
    filter->y3 = filter->y2;
    filter->y2 = filter->y1;
    filter->y1 = filtered_sample;

    return filtered_sample;
}
