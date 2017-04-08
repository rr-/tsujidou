#include <Python.h>
#include <string.h>
#include "stream.h"

Stream *stream_create_empty(void)
{
    Stream *stream = PyMem_RawMalloc(sizeof(Stream));
    if (!stream)
    {
        PyErr_SetNone(PyExc_MemoryError);
        return NULL;
    }
    stream->data = PyMem_RawMalloc(0);
    if (!stream->data)
    {
        PyMem_RawFree(stream);
        PyErr_SetNone(PyExc_MemoryError);
        return NULL;
    }
    stream->size = 0;
    stream->pos = 0;
    stream->owns_data = 1;
    return stream;
}

Stream *stream_create_for_data(unsigned char *data, size_t data_size)
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
    stream->owns_data = 0;
    return stream;
}

void stream_destroy(Stream *stream)
{
    assert(stream);
    if (stream->owns_data)
        PyMem_RawFree(stream->data);
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

static int ensure_stream_size(Stream *stream, size_t how_much)
{
    size_t new_size = stream->pos + how_much;
    if (new_size <= stream->size)
        return 1;
    unsigned char *new_data = PyMem_RawRealloc(stream->data, new_size);
    if (!new_data)
    {
        PyErr_SetNone(PyExc_MemoryError);
        return 0;
    }
    stream->data = new_data;
    stream->size = new_size;
    return 1;
}

int stream_write_data(Stream *stream, const unsigned char *data, size_t data_size)
{
    assert(stream);
    assert(ret);
    if (!ensure_stream_size(stream, data_size))
        return 0;
    memcpy(stream->data + stream->pos, data, data_size);
    stream->pos += data_size;
    return 1;
}

int stream_write_u8(Stream *stream, uint8_t data)
{
    assert(stream);
    assert(ret);
    if (!ensure_stream_size(stream, 1))
        return 0;
    *((uint8_t*)(stream->data + stream->pos)) = data;
    stream->pos++;
    return 1;
}

int stream_write_u32_le(Stream *stream, uint32_t data)
{
    assert(stream);
    assert(ret);
    if (!ensure_stream_size(stream, 4))
        return 0;
    *((uint32_t*)(stream->data + stream->pos)) = data;
    stream->pos += 4;
    return 1;
}
