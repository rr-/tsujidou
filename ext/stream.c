#include <Python.h>
#include <string.h>
#include "stream.h"

Stream *stream_create(unsigned char *data, size_t data_size)
{
    Stream *stream = PyMem_RawMalloc(sizeof(Stream));
    if (!stream)
    {
        PyErr_SetNone(PyExc_MemoryError);
        return NULL;
    }
    stream->data = data;
    stream->size = data_size;
    stream->pos = 0;
    return stream;
}

void stream_destroy(Stream *stream)
{
    assert(stream);
    PyMem_RawFree(stream);
}

int stream_read_data(Stream *stream, unsigned char *data, size_t data_size)
{
    assert(stream);
    assert(ret);
    if (stream->pos + data_size > stream->size)
    {
        PyErr_SetString(PyExc_ValueError, "Reading beyond EOF");
        return 0;
    }
    memcpy(data, stream->data + stream->pos, data_size);
    stream->pos += data_size;
    return 1;
}

int stream_read_u8(Stream *stream, uint8_t *ret)
{
    assert(stream);
    assert(ret);
    if (stream->pos + 1 > stream->size)
    {
        PyErr_SetString(PyExc_ValueError, "Reading beyond EOF");
        return 0;
    }
    *ret = *(const uint8_t*)(stream->data + stream->pos);
    stream->pos += 1;
    return 1;
}

int stream_read_u32_le(Stream *stream, uint32_t *ret)
{
    assert(stream);
    assert(ret);
    if (stream->pos + 4 > stream->size)
    {
        PyErr_SetString(PyExc_ValueError, "Reading beyond EOF");
        return 0;
    }
    *ret = *(const uint32_t*)(stream->data + stream->pos);
    stream->pos += 4;
    return 1;
}
