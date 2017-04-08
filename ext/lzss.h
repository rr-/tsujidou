#ifndef LZSS_H
#define LZSS_H

#include <stddef.h>

unsigned char *lzss_decompress(
    const unsigned char *input,
    const size_t input_size,
    const size_t output_size,
    unsigned char *dict,
    size_t *dict_pos);

unsigned char *lzss_compress(
    const unsigned char *input,
    const size_t input_size,
    size_t *output_size,
    unsigned char *dict,
    size_t *dict_pos);

#endif
