#include <stdio.h>
#include <string.h>
#include "constants.h"
#include "./utils/filter.h"
#include "./utils/wave.h"
#include "./utils/decode.h"
#include "./utils/ldpc.h"
#include "./utils/encode.h"
#include "./utils/unpack.h"
//#define N 174
//#define K 91
//#define K_BYTES 12 // (K + 7)/8
//
//const uint8_t icos7[7] = {3, 1, 4, 0, 6, 5, 2};
//const uint8_t graymap[8] = {0, 1, 3, 2, 5, 6, 4, 7};

const int kMax_candidates = 100;
const int kLDPC_iterations = 20;

const int kMax_decoded_messages = 50;
const int kMax_message_length = 20;

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        printf("main [usage] [filename] [message]\n");
        printf("usage: \n-d to decode a 15s wav file\n"
               "-e to encode a 15s wav file.\n"
               "message: if decode, it can be blank, or input what you want to encode.\n");
        return -1;
    }

    const char *wav_path = argv[2];
    if (!strcmp(argv[1], "-d"))  //do decode
    {
        int sample_rate = 12000;
        int num_samples = 15 * sample_rate;
        float signal[num_samples];
        int rc = load_wav(signal, &num_samples, &sample_rate, wav_path);
        if (rc < 0)
        {
            return -1;
        }
        normalize_signal(signal, num_samples);
        const float fsk_dev = 6.25f;    // tone deviation in Hz and symbol rate

        // Compute DSP parameters that depend on the sample rate
        const int num_bins = (int) (sample_rate / (2 * fsk_dev));
        const int block_size = 2 * num_bins;
        const int num_blocks = (num_samples - (block_size / 2) - block_size) / block_size;
//        LOG(LOG_INFO, "%d blocks, %d bins\n", num_blocks, num_bins);
        // Compute FFT over the whole signal and store it
        uint8_t power[num_blocks * 4 * num_bins];
        extract_power(signal, num_blocks, num_bins, power);

        // Find top candidates by Costas sync score and localize them in time and frequency
        struct Candidate candidate_list[kMax_candidates];
        int num_candidates = find_sync(power, num_blocks, num_bins, icos7, kMax_candidates, candidate_list);
        // TODO: sort the candidates by strongest sync first?

        // Go over candidates and attempt to decode messages
        char decoded[kMax_decoded_messages][kMax_message_length];
        int num_decoded = 0;
        for (int idx = 0; idx < num_candidates; ++idx)
        {
            struct Candidate *cand = &candidate_list[idx];
            float freq_hz = (cand->freq_offset + cand->freq_sub / 2.0f) * fsk_dev;
            float time_sec = (cand->time_offset + cand->time_sub / 2.0f) / fsk_dev;

            float log174[N];
            extract_likelihood(power, num_bins, cand, graymap, log174);

            // bp_decode() produces better decodes, uses way less memory
            uint8_t plain[N];
            int n_errors = 0;
            bp_decode(log174, kLDPC_iterations, plain, &n_errors);
            //ldpc_decode(log174, kLDPC_iterations, plain, &n_errors);

            if (n_errors > 0)
            {
//                LOG(LOG_DEBUG, "ldpc_decode() = %d (%.0f Hz)\n", n_errors, freq_hz);
                continue;
            }

            // Extract payload + CRC (first ft8::K bits)
            uint8_t a91[K_BYTES];
            pack_bits(plain, K, a91);

            // Extract CRC and check it
            uint16_t chksum = ((a91[9] & 0x07) << 11) | (a91[10] << 3) | (a91[11] >> 5);
            a91[9] &= 0xF8;
            a91[10] = 0;
            a91[11] = 0;
            uint16_t chksum2 = crc(a91, 96 - 14);
            if (chksum != chksum2)
            {
//                LOG(LOG_DEBUG, "Checksum: message = %04x, CRC = %04x\n", chksum, chksum2);
                continue;
            }

            char message[kMax_message_length];
            unpack77(a91, message);

            // Check for duplicate messages (TODO: use hashing)
            uint8_t found = 0;
            for (int i = 0; i < num_decoded; ++i)
            {
                if (0 == strcmp(decoded[i], message))
                {
                    found = 1;
                    break;
                }
            }

            if (!found && num_decoded < kMax_decoded_messages)
            {
                strcpy(decoded[num_decoded], message);
                ++num_decoded;

                // Fake WSJT-X-like output for now
                int snr = 0;    // TODO: compute SNR
                printf("000000 %3d %4.1f %4d ~  %s\n", cand->score, time_sec, (int) (freq_hz + 0.5f), message);
            }
        }
//        LOG(LOG_INFO, "Decoded %d messages\n", num_decoded);
        return 0;
    } else if (!strcmp(argv[1], "-e") && argc == 4) //do encode
    {

    } else
    {
        printf("Error instruction, or the message you input is wrong.\n");
        return -1;
    }

    printf("Hello, World!\n");
    return 0;
}