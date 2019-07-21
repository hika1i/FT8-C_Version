//
// Created by PYL on 2019/7/21.
//

#ifndef FT8_C_VERSION_WAVE_H
#define FT8_C_VERSION_WAVE_H

// Save signal in floating point format (-1 .. +1) as a WAVE file using 16-bit signed integers.
void save_wav(const float *signal, int num_samples, int sample_rate, const char *path);


// Load signal in floating point format (-1 .. +1) as a WAVE file using 16-bit signed integers.
int load_wav(float *signal, int *num_samples, int *sample_rate, const char *path);

#endif //FT8_C_VERSION_WAVE_H
