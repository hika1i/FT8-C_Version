//
// Created by PYL on 2019/7/21.
//

#ifndef FT8_C_VERSION_DECODE_H
#define FT8_C_VERSION_DECODE_H

#include <stdint.h>

struct Candidate
{
    int16_t score;
    int16_t time_offset;
    int16_t freq_offset;
    uint8_t time_sub;
    uint8_t freq_sub;
};

static float max2(float a, float b);

static float max4(float a, float b, float c, float d);

static void heapify_down(struct Candidate *heap, int heap_size);

static void heapify_up(struct Candidate *heap, int heap_size);

static void decode_symbol(const uint8_t *power, const uint8_t *code_map, int bit_idx, float *log174);

static void decode_multi_symbols(const uint8_t *power, int num_bins, int n_syms, const uint8_t *code_map, int bit_idx,
                                 float *log174);

// Localize top N candidates in frequency and time according to their sync strength (looking at Costas symbols)
// We treat and organize the candidate list as a min-heap (empty initially).
int find_sync(const uint8_t *power, int num_blocks, int num_bins, const uint8_t *sync_map, int num_candidates,
              struct Candidate *heap);

// Compute log likelihood log(p(1) / p(0)) of 174 message bits
// for later use in soft-decision LDPC decoding
void extract_likelihood(const uint8_t *power, int num_bins, const struct Candidate *cand, const uint8_t *code_map,
                        float *log174);

void extract_power(const float signal[], int num_blocks, int num_bins, uint8_t power[]);

void sort_sync(int num_candidates, struct Candidate *heap);

#endif //FT8_C_VERSION_DECODE_H
