//
// Created by PYL on 2019/7/21.
//
#include <math.h>
#include "filter.h"

float hann_i(int i, int n)
{
    float x = sinf((float) M_PI * i / (n - 1));
    return x * x;
}

float hamming_i(int i, int n)
{
    const float a0 = (float) 25 / 46;
    const float a1 = 1 - a0;

    float x1 = cosf(2 * (float) M_PI * i / (n - 1));
    return a0 - a1 * x1;
}

float blackman_i(int i, int n)
{
    const float alpha = 0.16f; // or 2860/18608
    const float a0 = (1 - alpha) / 2;
    const float a1 = 1.0f / 2;
    const float a2 = alpha / 2;

    float x1 = cosf(2 * (float) M_PI * i / (n - 1));
    //float x2 = cosf(4 * (float)M_PI * i / (n - 1));
    float x2 = 2 * x1 * x1 - 1; // Use double angle formula

    return a0 - a1 * x1 + a2 * x2;
}

void normalize_signal(float *signal, int num_samples)
{
    float max_amp = 1E-5f;
    for (int i = 0; i < num_samples; ++i)
    {
        float amp = fabsf(signal[i]);
        if (amp > max_amp)
        {
            max_amp = amp;
        }
    }
    for (int i = 0; i < num_samples; ++i)
    {
        signal[i] /= max_amp;
    }
}
