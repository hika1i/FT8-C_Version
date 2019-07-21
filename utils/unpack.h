//
// Created by PYL on 2019/7/21.
//

#ifndef FT8_C_VERSION_UNPACK_H
#define FT8_C_VERSION_UNPACK_H

#include <stdint.h>

// message should have at least 19 bytes allocated (18 characters + zero terminator)
int unpack77(const uint8_t *a77, char *message);

#endif //FT8_C_VERSION_UNPACK_H
