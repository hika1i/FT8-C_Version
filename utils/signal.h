//
// Created by PYL on 2019/7/21.
//

#ifndef FT8_C_VERSION_SIGNAL_H
#define FT8_C_VERSION_SIGNAL_H

float hann_i(int i, int n);

float hamming_i(int i, int n);

float blackman_i(int i, int n);

void normalize_signal(float *signal, int num_samples);

#endif //FT8_C_VERSION_SIGNAL_H
