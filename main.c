#include <stdio.h>
#include <string.h>
#include "constants.h"
#include "utils/signal.h"
#include "./utils/wave.h"
#include "./utils/decode.h"
#include "./utils/ldpc.h"
#include "./utils/encode.h"
#include "./utils/unpack.h"
#include "./utils/pack.h"
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
        int num_samples = 15 * sample_rate; //15s audio
        float signal[num_samples];
        int rc = load_wav(signal, &num_samples, &sample_rate, wav_path);
        if (rc < 0)
        {
            printf("Load wave file failed. %d\n", rc);
            return -1;
        }
        normalize_signal(signal, num_samples);
        const float fsk_dev = 6.25f;    // tone deviation in Hz and symbol rate 50Hz/8 tone
        printf("Sample rate is %d, num_samples is %d.\n", sample_rate, num_samples);
        // Compute DSP parameters that depend on the sample rate
        const int num_bins = (int) (sample_rate / (2 * fsk_dev));
        const int block_size = 2 * num_bins;
        const int num_blocks = (num_samples - (block_size / 2) - block_size) / block_size;
//        LOG(LOG_INFO, "%d blocks, %d bins\n", num_blocks, num_bins);
        printf("Blocks are %d, Bins are %d.\n", num_blocks, num_bins);
        // Compute FFT over the whole signal and store it
        uint8_t power[num_blocks * 4 * num_bins];
        extract_power(signal, num_blocks, num_bins, power);

        // Find top candidates by Costas sync score and localize them in time and frequency
        struct Candidate candidate_list[kMax_candidates];
        int num_candidates = find_sync(power, num_blocks, num_bins, icos7, kMax_candidates, candidate_list);
        // TODO: sort the candidates by strongest sync first?
        sort_sync(num_candidates, candidate_list);
        // Go over candidates and attempt to decode messages
        char decoded[kMax_decoded_messages][kMax_message_length];
        int num_decoded = 0;
        printf("UTC cand_score dB    DT   Freq   Mesg\n");
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
                printf("ldpc_decode() = %d (%.0f Hz)\n", n_errors, freq_hz);
                continue;
            }

            // Extract payload + CRC (first K bits)
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
                float snr = 0;    // compute SNR
                snr = (cand->score / 255.0 - 1) * 24;
                printf("000000 %3d  %.1f dB %4.1f %4d ~  %s\n", cand->score, snr, time_sec, (int) (freq_hz + 0.5f), message);
            }
        }
//        LOG(LOG_INFO, "Decoded %d messages\n", num_decoded);
        return 0;
    }
    //do encode
    // TODO: free-text messages(13 characters from alphabets)
    // TODO: contest mode

    else if (!strcmp(argv[1], "-e") && argc >= 4)
    {
        const char *wav_path = argv[2];
        const char *message = argv[3];
        printf("Input messages are: %s\n", message);
        // First, pack the text data into binary message
        uint8_t packed[K_BYTES];
        //int rc = packmsg(message, packed);
        int rc = pack77(message, packed);
        if (rc < 0)
        {
            printf("Cannot parse message!\n");
            printf("RC = %d\n", rc);
            return -2;
        }

        printf("Packed data: ");
        for (int j = 0; j < 10; ++j)
        {
            printf("%02x ", packed[j]);
        }
        printf("\n");

        // Second, encode the binary message as a sequence of FSK tones
        uint8_t tones[NN];          // FT8_NN = 79, lack of better name at the moment

        genft8(packed, tones);

        printf("FSK tones: ");
        for (int j = 0; j < NN; ++j)
        {
            printf("%d", tones[j]);
        }
        printf("\n");

        // Third, convert the FSK tones into an audio signal
        const int sample_rate = 12000;
        const float symbol_rate = 6.25f;
        const int num_samples = (int)(0.5f + NN / symbol_rate * sample_rate);
        const int num_silence = (15 * sample_rate - num_samples) / 2;
        float signal[num_silence + num_samples + num_silence];
        for (int i = 0; i < num_silence + num_samples + num_silence; i++)
        {
            signal[i] = 0;
        }

        synth_fsk(tones, NN, 1000, symbol_rate, symbol_rate, sample_rate, signal + num_silence);
        save_wav(signal, num_silence + num_samples + num_silence, sample_rate, wav_path);

        return 0;
    } else
    {
        printf("Error instruction, or the message you input is wrong.\n");
        return -1;
    }

    return 0;
}