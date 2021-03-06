#ifndef STREAM_H
#define STREAM_H

#include <Python.h>
#include <string.h>

typedef struct
{
    unsigned char *data;
    size_t size;
    size_t pos;
    int owns_data;
} Stream;

Stream *stream_create_empty(void);
Stream *stream_create_for_data(unsigned char *data, size_t data_size);
void stream_destroy(Stream *stream);

int stream_read_data(Stream *stream, unsigned char *data, size_t data_size);
int stream_read_u8(Stream *stream, uint8_t *ret);
int stream_read_u32_le(Stream *stream, uint32_t *ret);

int stream_write_data(Stream *stream, const unsigned char *data, size_t data_size);
int stream_write_u8(Stream *stream, uint8_t data);
int stream_write_u32_le(Stream *stream, uint32_t data);

#endif
