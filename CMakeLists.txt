cmake_minimum_required(VERSION 3.13)
project(FT8_C_Version C)

set(CMAKE_C_STANDARD 99)

add_executable(FT8_C_Version main.c constants.h fft/kiss_fftr.c fft/kiss_fftr.h fft/kiss_ftt.c fft/kiss_ftt.h fft/_kiss_fft_guts.h utils/filter.c utils/filter.h utils/wave.c utils/wave.h utils/decode.c utils/decode.h utils/ldpc.c utils/ldpc.h utils/encode.c utils/encode.h utils/unpack.c utils/unpack.h utils/text.c utils/text.h constants.c)