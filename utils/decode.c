//
// Created by PYL on 2019/7/21.
//

#include "decode.h"
#include "../constants.h"
#include <stdlib.h>
#include "filter.h"
#include "../fft/kiss_fftr.h"

// Localize top N candidates in frequency and time according to their sync strength (looking at Costas symbols)
// We treat and organize the candidate list as a min-heap (empty initially).
int find_sync(const uint8_t *power, int num_blocks, int num_bins, const uint8_t *sync_map, int num_candidates,
              struct Candidate *heap)
{
    int heap_size = 0;

    // Here we allow time offsets that exceed signal boundaries, as long as we still have all data bits.
    // I.e. we can afford to skip the first 7 or the last 7 Costas symbols, as long as we track how many
    // sync symbols we included in the score, so the score is averaged.
    for (int alt = 0; alt < 4; ++alt)
    {
        for (int time_offset = -7; time_offset < num_blocks - NN + 7; ++time_offset)
        {
            for (int freq_offset = 0; freq_offset < num_bins - 8; ++freq_offset)
            {
                int score = 0;

                // Compute average score over sync symbols (m+k = 0-7, 36-43, 72-79)
                int num_symbols = 0;
                for (int m = 0; m <= 72; m += 36)
                {
                    for (int k = 0; k < 7; ++k)
                    {
                        // Check for time boundaries
                        if (time_offset + k + m < 0) continue;
                        if (time_offset + k + m >= num_blocks) break;

                        int offset = ((time_offset + k + m) * 4 + alt) * num_bins + freq_offset;
                        const uint8_t *p8 = power + offset;

                        score += 8 * p8[sync_map[k]] -
                                 p8[0] - p8[1] - p8[2] - p8[3] -
                                 p8[4] - p8[5] - p8[6] - p8[7];

                        // int sm = sync_map[k];
                        // score += 4 * (int)p8[sm];
                        // if (sm > 0) score -= p8[sm - 1];
                        // if (sm < 7) score -= p8[sm + 1];
                        // if (k > 0) score -= p8[sm - 4 * num_bins];
                        // if (k < 6) score -= p8[sm + 4 * num_bins];

                        ++num_symbols;
                    }
                }
                score /= num_symbols;

                // If the heap is full AND the current candidate is better than
                // the worst in the heap, we remove the worst and make space
                if (heap_size == num_candidates && score > heap[0].score)
                {
                    heap[0] = heap[heap_size - 1];
                    --heap_size;

                    heapify_down(heap, heap_size);
                }

                // If there's free space in the heap, we add the current candidate
                if (heap_size < num_candidates)
                {
                    heap[heap_size].score = score;
                    heap[heap_size].time_offset = time_offset;
                    heap[heap_size].freq_offset = freq_offset;
                    heap[heap_size].time_sub = alt / 2;
                    heap[heap_size].freq_sub = alt % 2;
                    ++heap_size;

                    heapify_up(heap, heap_size);
                }
            }
        }
    }

    return heap_size;
}

// Compute log likelihood log(p(1) / p(0)) of 174 message bits
// for later use in soft-decision LDPC decoding
void extract_likelihood(const uint8_t *power, int num_bins, const struct Candidate *cand, const uint8_t *code_map,
                        float *log174)
{
    int offset = (cand->time_offset * 4 + cand->time_sub * 2 + cand->freq_sub) * num_bins + cand->freq_offset;

    // Go over FSK tones and skip Costas sync symbols
    const int n_syms = 1;
    const int n_bits = 3 * n_syms;
    const int n_tones = (1 << n_bits);
    for (int k = 0; k < ND; k += n_syms)
    {
        int sym_idx = (k < ND / 2) ? (k + 7) : (k + 14);
        int bit_idx = 3 * k;

        // Pointer to 8 bins of the current symbol
        const uint8_t *ps = power + (offset + sym_idx * 4 * num_bins);

        decode_symbol(ps, code_map, bit_idx, log174);
    }

    // Compute the variance of log174
    float sum = 0;
    float sum2 = 0;
    float inv_n = 1.0f / N;
    for (int i = 0; i < N; ++i)
    {
        sum += log174[i];
        sum2 += log174[i] * log174[i];
    }
    float variance = (sum2 - sum * sum * inv_n) * inv_n;

    // Normalize log174 such that sigma = 2.83 (Why? It's in WSJT-X, ft8b.f90)
    // Seems to be 2.83 = sqrt(8). Experimentally sqrt(16) works better.
    float norm_factor = sqrtf(16.0f / variance);
    for (int i = 0; i < N; ++i)
    {
        log174[i] *= norm_factor;
    }
}

static float max2(float a, float b)
{
    return (a >= b) ? a : b;
}

static float max4(float a, float b, float c, float d)
{
    return max2(max2(a, b), max2(c, d));
}

static void heapify_down(struct Candidate *heap, int heap_size)
{
    // heapify from the root down
    int current = 0;
    while (1)
    {
        int largest = current;
        int left = 2 * current + 1;
        int right = left + 1;

        if (left < heap_size && heap[left].score < heap[largest].score)
        {
            largest = left;
        }
        if (right < heap_size && heap[right].score < heap[largest].score)
        {
            largest = right;
        }
        if (largest == current)
        {
            break;
        }

        struct Candidate tmp = heap[largest];
        heap[largest] = heap[current];
        heap[current] = tmp;
        current = largest;
    }
}

static void heapify_up(struct Candidate *heap, int heap_size)
{
    // heapify from the last node up
    int current = heap_size - 1;
    while (current > 0)
    {
        int parent = (current - 1) / 2;
        if (heap[current].score >= heap[parent].score)
        {
            break;
        }

        struct Candidate tmp = heap[parent];
        heap[parent] = heap[current];
        heap[current] = tmp;
        current = parent;
    }
}


// Compute unnormalized log likelihood log(p(1) / p(0)) of 3 message bits (1 FSK symbol)
static void decode_symbol(const uint8_t *power, const uint8_t *code_map, int bit_idx, float *log174)
{
    // Cleaned up code for the simple case of n_syms==1
    float s2[8];

    for (int j = 0; j < 8; ++j)
    {
        s2[j] = (float) power[code_map[j]];
    }

    log174[bit_idx + 0] = max4(s2[4], s2[5], s2[6], s2[7]) - max4(s2[0], s2[1], s2[2], s2[3]);
    log174[bit_idx + 1] = max4(s2[2], s2[3], s2[6], s2[7]) - max4(s2[0], s2[1], s2[4], s2[5]);
    log174[bit_idx + 2] = max4(s2[1], s2[3], s2[5], s2[7]) - max4(s2[0], s2[2], s2[4], s2[6]);
}


// Compute unnormalized log likelihood log(p(1) / p(0)) of bits corresponding to several FSK symbols at once
static void decode_multi_symbols(const uint8_t *power, int num_bins, int n_syms, const uint8_t *code_map, int bit_idx,
                                 float *log174)
{
    // The following section implements what seems to be multiple-symbol decode at one go,
    // corresponding to WSJT-X's ft8b.f90. Experimentally found not to be any better than
    // 1-symbol decode.

    const int n_bits = 3 * n_syms;
    const int n_tones = (1 << n_bits);

    float s2[n_tones];

    for (int j = 0; j < n_tones; ++j)
    {
        int j1 = j & 0x07;
        if (n_syms == 1)
        {
            s2[j] = (float) power[code_map[j1]];
            continue;
        }
        int j2 = (j >> 3) & 0x07;
        if (n_syms == 2)
        {
            s2[j] = (float) power[code_map[j2]];
            s2[j] += (float) power[code_map[j1] + 4 * num_bins];
            continue;
        }
        int j3 = (j >> 6) & 0x07;
        s2[j] = (float) power[code_map[j3]];
        s2[j] += (float) power[code_map[j2] + 4 * num_bins];
        s2[j] += (float) power[code_map[j1] + 8 * num_bins];
    }
    // No need to go back to linear scale any more. Works better in dB.
    // for (int j = 0; j < n_tones; ++j) {
    //     s2[j] = powf(10.0f, 0.1f * s2[j]);
    // }

    // Extract bit significance (and convert them to float)
    // 8 FSK tones = 3 bits
    for (int i = 0; i < n_bits; ++i)
    {
        if (bit_idx + i >= N)
        {
            // Respect array size
            break;
        }

        uint16_t mask = (n_tones >> (i + 1));
        float max_zero = -1000, max_one = -1000;
        for (int n = 0; n < n_tones; ++n)
        {
            if (n & mask)
            {
                max_one = max2(max_one, s2[n]);
            } else
            {
                max_zero = max2(max_zero, s2[n]);
            }
        }

        log174[bit_idx + i] = max_one - max_zero;
    }
}

// Compute FFT magnitudes (log power) for each timeslot in the signal
void extract_power(const float signal[], int num_blocks, int num_bins, uint8_t power[])
{
    const int block_size = 2 * num_bins;      // Average over 2 bins per FSK tone
    const int nfft = 2 * block_size;          // We take FFT of two blocks, advancing by one
    const float fft_norm = 2.0f / nfft;

    float window[nfft];
    for (int i = 0; i < nfft; ++i)
    {
        window[i] = blackman_i(i, nfft);
    }

    size_t fft_work_size;
    kiss_fftr_alloc(nfft, 0, 0, &fft_work_size);

//    LOG(LOG_INFO, "N_FFT = %d\n", nfft);
//    LOG(LOG_INFO, "FFT work area = %lu\n", fft_work_size);

    void *fft_work = malloc(fft_work_size);
    kiss_fftr_cfg fft_cfg = kiss_fftr_alloc(nfft, 0, fft_work, &fft_work_size);

    int offset = 0;
    float max_mag = -100.0f;
    for (int i = 0; i < num_blocks; ++i)
    {
        // Loop over two possible time offsets (0 and block_size/2)
        for (int time_sub = 0; time_sub <= block_size / 2; time_sub += block_size / 2)
        {
            kiss_fft_scalar timedata[nfft];
            kiss_fft_cpx freqdata[nfft / 2 + 1];
            float mag_db[nfft / 2 + 1];

            // Extract windowed signal block
            for (int j = 0; j < nfft; ++j)
            {
                timedata[j] = window[j] * signal[(i * block_size) + (j + time_sub)];
            }

            kiss_fftr(fft_cfg, timedata, freqdata);

            // Compute log magnitude in decibels
            for (int j = 0; j < nfft / 2 + 1; ++j)
            {
                float mag2 = (freqdata[j].i * freqdata[j].i + freqdata[j].r * freqdata[j].r);
                mag_db[j] = 10.0f * log10f(1E-10f + mag2 * fft_norm * fft_norm);
            }

            // Loop over two possible frequency bin offsets (for averaging)
            for (int freq_sub = 0; freq_sub < 2; ++freq_sub)
            {
                for (int j = 0; j < num_bins; ++j)
                {
                    float db1 = mag_db[j * 2 + freq_sub];
                    float db2 = mag_db[j * 2 + freq_sub + 1];
                    float db = (db1 + db2) / 2;

                    // Scale decibels to unsigned 8-bit range and clamp the value
                    int scaled = (int) (2 * (db + 120));
                    power[offset] = (scaled < 0) ? 0 : ((scaled > 255) ? 255 : scaled);
                    ++offset;

                    if (db > max_mag) max_mag = db;
                }
            }
        }
    }
//    LOG(LOG_INFO, "Max magnitude: %.1f dB\n", max_mag);
    free(fft_work);
}
