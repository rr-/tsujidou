#include <Python.h>
#include "lzss.h"

unsigned char *lzss_decompress(
    const unsigned char *input,
    const size_t input_size,
    const size_t output_size,
    unsigned char *dict,
    size_t *dict_pos)
{
    assert(input);
    assert(dict);
    assert(dict_pos);

    unsigned char *output = PyMem_RawMalloc(output_size);
    if (!output)
    {
        PyErr_SetNone(PyExc_MemoryError);
        return NULL;
    }

    unsigned char *output_ptr = output;
    const unsigned char *input_ptr = input;
    const unsigned char *input_end = input_ptr + input_size;
    const unsigned char *output_end = output_ptr + output_size;

    int flags = 0;
    while (input_ptr < input_end)
    {
        flags >>= 1;
        if ((flags & 0x100) != 0x100)
        {
            if (input_ptr >= input_end)
                return output;
            flags = *input_ptr++ | 0xFF00;
        }

        if ((flags & 1) == 1)
        {
            if (input_ptr >= input_end)
                return output;
            unsigned char x0 = *input_ptr++;
            if (input_ptr >= input_end)
                return output;
            unsigned char x1 = *input_ptr++;
            size_t lookbehind_pos = x0 | ((x1 & 0xF) << 8);
            size_t lookbehind_size = 3 + ((x1 & 0xF0) >> 4);
            if (lookbehind_size == 18)
            {
                if (input_ptr >= input_end)
                    return output;
                lookbehind_size += *input_ptr++;
            }

            for (size_t j = 0; j < lookbehind_size; j++)
            {
                unsigned char c = dict[lookbehind_pos];
                if (output_ptr >= output_end)
                    return output;
                *output_ptr++ = c;
                dict[*dict_pos] = c;
                (*dict_pos)++;
                (*dict_pos) &= 0xFFF;
                lookbehind_pos++;
                lookbehind_pos &= 0xFFF;
            }
        }
        else
        {
            if (input_ptr >= input_end)
                return output;
            unsigned char c = *input_ptr++;
            if (output_ptr >= output_end)
                return output;
            *output_ptr++ = c;
            dict[*dict_pos] = c;
            (*dict_pos)++;
            (*dict_pos) &= 0xFFF;
        }
    }

    return output;
}

// dummy implementation
unsigned char *lzss_compress(
    const unsigned char *input,
    const size_t input_size,
    size_t *output_size,
    unsigned char *dict,
    size_t *dict_pos)
{
    assert(input);
    assert(output_size);

    *output_size = input_size + input_size / 8;
    unsigned char *output = PyMem_RawMalloc(*output_size);
    if (!output)
    {
        PyErr_SetNone(PyExc_MemoryError);
        return NULL;
    }

    unsigned char *output_ptr = output;
    for (size_t i = 0; i < input_size; i++)
    {
        if (i % 8 == 0)
            *output_ptr++ = 0;
        *output_ptr++ = input[i];
    }

    return output;
}
