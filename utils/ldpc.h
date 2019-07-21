//
// Created by PYL on 2019/7/21.
//

#ifndef FT8_C_VERSION_LDPC_H
#define FT8_C_VERSION_LDPC_H

#include <stdint.h>

// codeword is 174 log-likelihoods.
// plain is a return value, 174 ints, to be 0 or 1.
// iters is how hard to try.
// ok == 87 means success.
void ldpc_decode(float codeword[], int max_iters, uint8_t plain[], int *ok);

void bp_decode(float codeword[], int max_iters, uint8_t plain[], int *ok);

// Packs a string of bits each represented as a zero/non-zero byte in plain[],
// as a string of packed bits starting from the MSB of the first byte of packed[]
void pack_bits(const uint8_t plain[], int num_bits, uint8_t packed[]);

#endif //FT8_C_VERSION_LDPC_H
