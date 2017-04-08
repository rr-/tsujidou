#include <Python.h>
#include <string.h>
#include "stream.h"
#include "lzss.h"
#include "pixel.h"

#define MAGIC "TLG5.0\x00raw\x1A"
#define MAGIC_SIZE 11

typedef struct
{
    uint8_t channel_count;
    uint32_t image_width;
    uint32_t image_height;
    uint32_t block_height;
} Tlg5Header;

typedef struct
{
    unsigned char *data;
    size_t data_size;
} Tlg5BlockInfo;

static Tlg5BlockInfo *tlg5_block_info_create_empty(void)
{
    Tlg5BlockInfo *block_info = PyMem_RawMalloc(sizeof(Tlg5BlockInfo));
    if (!block_info)
    {
        PyErr_SetNone(PyExc_MemoryError);
        return NULL;
    }
    block_info->data = NULL;
    block_info->data_size = 0;
    return block_info;
}

static Tlg5BlockInfo *tlg5_block_info_create_for_data(const size_t data_size)
{
    Tlg5BlockInfo *block_info = PyMem_RawMalloc(sizeof(Tlg5BlockInfo));
    if (!block_info)
    {
        PyErr_SetNone(PyExc_MemoryError);
        return NULL;
    }
    block_info->data = PyMem_RawMalloc(data_size);
    if (!block_info->data)
    {
        PyMem_RawFree(block_info);
        PyErr_SetNone(PyExc_MemoryError);
        return NULL;
    }
    block_info->data_size = data_size;
    return block_info;
}

static void tlg5_block_info_destroy(Tlg5BlockInfo *block_info)
{
    assert(block_info);
    PyMem_RawFree(block_info->data);
    PyMem_RawFree(block_info);
}

static int tlg5_block_info_read(
    Tlg5BlockInfo *block_info,
    Stream *stream,
    const Tlg5Header *header,
    uint8_t *dict,
    size_t *dict_pos)
{
    assert(block_info);
    assert(stream);
    assert(header);
    assert(dict);
    assert(dict_pos);

    uint8_t mark;
    uint32_t data_comp_size;
    unsigned char *data_comp = NULL;
    unsigned char *data_orig = NULL;
    int ret = 0;

    const size_t data_orig_size = header->image_width * header->block_height;

    if (!stream_read_u8(stream, &mark))
        goto end;

    if (!stream_read_u32_le(stream, &data_comp_size))
        goto end;

    if (mark == 0)
    {
        data_comp = PyMem_RawMalloc(data_comp_size);
        if (!data_comp)
        {
            PyErr_SetNone(PyExc_MemoryError);
            goto end;
        }
        if (!stream_read_data(stream, data_comp, data_comp_size))
            goto end;

        data_orig = lzss_decompress(
            data_comp, data_comp_size, data_orig_size, dict, dict_pos);
        if (!data_orig)
            goto end;
    }
    else
    {
        data_orig = PyMem_RawMalloc(data_comp_size);
        if (!data_orig)
        {
            PyErr_SetNone(PyExc_MemoryError);
            goto end;
        }
        if (!stream_read_data(stream, data_orig, data_orig_size))
            goto end;
    }

    block_info->data = data_orig;
    block_info->data_size = data_orig_size;

    ret = 1;
end:
    if (data_comp)
        PyMem_RawFree(data_comp);
    return ret;
}

static int tlg5_block_info_write(
    Tlg5BlockInfo *block_info,
    Stream *stream,
    const Tlg5Header *header,
    uint8_t *dict,
    size_t *dict_pos)
{
    assert(block_info);
    assert(stream);
    assert(header);
    assert(dict);
    assert(dict_pos);

    int ret = 0;

    const unsigned char *data_orig = block_info->data;
    const size_t data_orig_size = block_info->data_size;
    unsigned char *data_comp = NULL;
    size_t data_comp_size = 0;

    data_comp = lzss_compress(
        block_info->data,
        block_info->data_size,
        &data_comp_size,
        dict,
        dict_pos);

    // for some reason, even though the dummy lzss compression seems to work
    // correctly, the game crashes... therefore we'll stick to raw mode
    if (0 && data_comp_size < data_orig_size)
    {
        if (!data_comp) goto end;
        if (!stream_write_u8(stream, 0)) goto end;
        if (!stream_write_u32_le(stream, data_comp_size)) goto end;
        if (!stream_write_data(stream, data_comp, data_comp_size)) goto end;
    }
    else
    {
        if (!stream_write_u8(stream, 1)) goto end;
        if (!stream_write_u32_le(stream, data_orig_size)) goto end;
        if (!stream_write_data(stream, data_orig, data_orig_size)) goto end;
    }


    ret = 1;
end:
    if (data_comp)
        PyMem_RawFree(data_comp);
    return ret;
}

static int tlg5_header_read(Stream *stream, Tlg5Header *header)
{
    assert(stream);
    assert(header);
    if (!stream_read_u8(stream, &header->channel_count)) return 0;
    if (!stream_read_u32_le(stream, &header->image_width)) return 0;
    if (!stream_read_u32_le(stream, &header->image_height)) return 0;
    if (!stream_read_u32_le(stream, &header->block_height)) return 0;
    return 1;
}

static int tlg5_header_write(Stream *stream, Tlg5Header *header)
{
    assert(stream);
    assert(header);
    if (!stream_write_u8(stream, header->channel_count)) return 0;
    if (!stream_write_u32_le(stream, header->image_width)) return 0;
    if (!stream_write_u32_le(stream, header->image_height)) return 0;
    if (!stream_write_u32_le(stream, header->block_height)) return 0;
    return 1;
}

static int tlg5_load_pixel_block_row(
    Pixel *image_data,
    const size_t image_data_size,
    Tlg5BlockInfo **block_data,
    const Tlg5Header *header,
    size_t block_y)
{
    size_t max_y = block_y + header->block_height;
    if (max_y > header->image_height)
        max_y = header->image_height;
    int use_alpha = header->channel_count == 4;

    for (size_t y = block_y; y < max_y; y++)
    {
        size_t block_y_shift = (y - block_y) * header->image_width;
        Pixel prev_pixel = {0, 0, 0, 0};

        for (size_t x = 0; x < header->image_width; x++)
        {
            Pixel pixel;
            pixel.b = block_data[0]->data[block_y_shift + x];
            pixel.g = block_data[1]->data[block_y_shift + x];
            pixel.r = block_data[2]->data[block_y_shift + x];
            pixel.a = use_alpha
                ? block_data[3]->data[block_y_shift + x]
                : 0xFF;
            pixel.b += pixel.g;
            pixel.r += pixel.g;

            prev_pixel.r += pixel.r;
            prev_pixel.g += pixel.g;
            prev_pixel.b += pixel.b;
            prev_pixel.a += pixel.a;

            Pixel *target_pixel = image_data + y * header->image_width + x;
            if (target_pixel >= image_data + image_data_size/sizeof(Pixel))
            {
                PyErr_SetString(PyExc_ValueError, "Corrupt data");
                return 0;
            }

            target_pixel->r = prev_pixel.r;
            target_pixel->g = prev_pixel.g;
            target_pixel->b = prev_pixel.b;
            target_pixel->a = prev_pixel.a;
            if (y > 0)
            {
                Pixel *top_pixel = target_pixel - header->image_width;
                target_pixel->r += top_pixel->r;
                target_pixel->g += top_pixel->g;
                target_pixel->b += top_pixel->b;
                target_pixel->a += top_pixel->a;
            }
            if (!use_alpha)
                image_data[y * header->image_width + x].a = 0xFF;
        }
    }
    return 1;
}

static int tlg5_save_pixel_block_row(
    const Pixel *image_data,
    const size_t image_data_size,
    Tlg5BlockInfo **block_data,
    const Tlg5Header *header,
    size_t block_y)
{
    size_t max_y = block_y + header->block_height;
    if (max_y > header->image_height)
        max_y = header->image_height;

    for (size_t y = block_y; y < max_y; y++)
    {
        size_t block_y_shift = (y - block_y) * header->image_width;
        Pixel prev_pixel = {0, 0, 0, 0};

        for (size_t x = 0; x < header->image_width; x++)
        {
            const Pixel *source_pixel =
                image_data + y * header->image_width + x;
            if (source_pixel >= image_data + image_data_size/sizeof(Pixel))
            {
                PyErr_SetString(PyExc_ValueError, "Corrupt data");
                return 0;
            }

            Pixel pixel = *source_pixel;

            if (y > 0)
            {
                const Pixel *top_pixel = source_pixel - header->image_width;
                pixel.r -= top_pixel->r;
                pixel.g -= top_pixel->g;
                pixel.b -= top_pixel->b;
                pixel.a -= top_pixel->a;
            }

            pixel.r -= prev_pixel.r;
            pixel.g -= prev_pixel.g;
            pixel.b -= prev_pixel.b;
            pixel.a -= prev_pixel.a;

            prev_pixel.r += pixel.r;
            prev_pixel.g += pixel.g;
            prev_pixel.b += pixel.b;
            prev_pixel.a += pixel.a;

            pixel.b -= pixel.g;
            pixel.r -= pixel.g;
            block_data[0]->data[block_y_shift + x] = pixel.b;
            block_data[1]->data[block_y_shift + x] = pixel.g;
            block_data[2]->data[block_y_shift + x] = pixel.r;
            block_data[3]->data[block_y_shift + x] = pixel.a;
        }
    }
    return 1;
}

static PyObject *tlg5_decode(PyObject *self, PyObject *args)
{
    Stream *stream = NULL;
    Pixel *image_data = NULL;
    Py_buffer input = {0};
    PyObject *output = NULL;
    Tlg5Header header;
    Tlg5BlockInfo *block_info[4] = {NULL, NULL, NULL, NULL};

    if (!PyArg_ParseTuple(args, "y*", &input))
        goto end;

    stream = stream_create_for_data(input.buf, input.len);
    if (!stream)
        goto end;

    if (memcmp(stream->data, MAGIC, MAGIC_SIZE))
    {
        PyErr_SetString(PyExc_ValueError, "Not a TLG5 image");
        goto end;
    }
    stream->pos += MAGIC_SIZE;

    if (!tlg5_header_read(stream, &header))
        goto end;
    if (header.channel_count != 3 && header.channel_count != 4)
    {
        PyErr_SetString(PyExc_ValueError, "Unsupported channel count");
        goto end;
    }

    const size_t image_data_size =
        header.image_height * header.image_width * 4;
    image_data = PyMem_RawMalloc(image_data_size);
    if (!image_data)
    {
        PyErr_SetNone(PyExc_MemoryError);
        goto end;
    }

    // ignore block sizes
    size_t block_count = (header.image_height - 1) / header.block_height + 1;
    stream->pos += 4 * block_count;

    for (int channel = 0; channel < 4; channel++)
    {
        block_info[channel] = tlg5_block_info_create_empty();
        if (!block_info[channel])
            goto end;
    }

    unsigned char dict[4096] = {0};
    size_t dict_pos = 0;

    for (size_t y = 0; y < header.image_height; y += header.block_height)
    {
        for (int channel = 0; channel < header.channel_count; channel++)
        {
            if (!tlg5_block_info_read(
                block_info[channel], stream, &header, dict, &dict_pos))
            {
                goto end;
            }
        }
        if (!tlg5_load_pixel_block_row(
            image_data, image_data_size, block_info, &header, y))
        {
            goto end;
        }
    }
    for (int channel = 0; channel < 4; channel++)
    {
        tlg5_block_info_destroy(block_info[channel]);
        block_info[channel] = NULL;
    }

    PyObject *output_image_width = PyLong_FromLong(header.image_width);
    PyObject *output_image_height = PyLong_FromLong(header.image_height);
    PyObject *output_image_data = PyBytes_FromStringAndSize(
        (char*)image_data, image_data_size);
    output = PyTuple_Pack(
        3,
        output_image_width,
        output_image_height,
        output_image_data);
    Py_DECREF(output_image_width);
    Py_DECREF(output_image_height);
    Py_DECREF(output_image_data);

end:
    for (int channel = 0; channel < 4; channel++)
        if (block_info[channel])
            tlg5_block_info_destroy(block_info[channel]);
    if (image_data) PyMem_RawFree(image_data);
    if (stream) stream_destroy(stream);
    PyBuffer_Release(&input);
    return output;
}

static PyObject *tlg5_encode(PyObject *self, PyObject *args)
{
    Tlg5BlockInfo *block_info[4] = {NULL, NULL, NULL, NULL};
    int input_image_width;
    int input_image_height;
    Py_buffer input_image_data = {0};
    Stream *stream = NULL;
    PyObject *output = NULL;

    if (!PyArg_ParseTuple(
            args,
            "iiy*",
            &input_image_width,
            &input_image_height,
            &input_image_data))
    {
        goto end;
    }

    if (input_image_data.len != input_image_width * input_image_height * 4)
    {
        PyErr_SetString(PyExc_ValueError, "Invalid data size");
        goto end;
    }

    stream = stream_create_empty();
    if (!stream)
        goto end;

    if (!stream_write_data(stream, (unsigned char*)MAGIC, MAGIC_SIZE))
        goto end;

    Tlg5Header header;
    header.channel_count = 4;
    header.image_width = input_image_width;
    header.image_height = input_image_height;
    header.block_height = 16;
    if (!tlg5_header_write(stream, &header))
        goto end;

    size_t block_sizes_offset = stream->pos;
    size_t block_count = (header.image_height - 1) / header.block_height + 1;
    for (size_t i = 0; i < block_count; i++)
        if (!stream_write_u32_le(stream, 0))
            goto end;

    for (int channel = 0; channel < 4; channel++)
    {
        block_info[channel] = tlg5_block_info_create_for_data(
            header.image_width * header.block_height);
        if (!block_info[channel])
            goto end;
    }

    unsigned char dict[4096] = {0};
    size_t dict_pos = 0;

    for (size_t y = 0; y < header.image_height; y += header.block_height)
    {
        size_t old_pos = stream->pos;
        for (int channel = 0; channel < header.channel_count; channel++)
        {
            if (!tlg5_save_pixel_block_row(
                input_image_data.buf,
                input_image_data.len,
                block_info,
                &header,
                y))
            {
                goto end;
            }

            if (!tlg5_block_info_write(
                block_info[channel],
                stream,
                &header,
                dict,
                &dict_pos))
            {
                goto end;
            }
        }
        size_t collective_size = stream->pos - old_pos;

        old_pos = stream->pos;
        stream->pos = block_sizes_offset + 4 * y / header.block_height;
        if (!stream_write_u32_le(stream, collective_size))
            goto end;
        stream->pos = old_pos;
    }
    for (int channel = 0; channel < 4; channel++)
    {
        tlg5_block_info_destroy(block_info[channel]);
        block_info[channel] = NULL;
    }

    output = PyBytes_FromStringAndSize(
        (char*)stream->data, stream->size);

end:
    for (int channel = 0; channel < 4; channel++)
        if (block_info[channel])
            tlg5_block_info_destroy(block_info[channel]);
    if (stream)
        stream_destroy(stream);
    PyBuffer_Release(&input_image_data);
    return output;
}

static PyMethodDef Methods[] = {
    {"decode_tlg_5", tlg5_decode, METH_VARARGS, "Decode a tlg5 image"},
    {"encode_tlg_5", tlg5_encode, METH_VARARGS, "Encode a tlg5 image"},
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef module_definition = {
   PyModuleDef_HEAD_INIT, "lib.tlg.tlg5", NULL, -1, Methods,
};

PyMODINIT_FUNC PyInit_tlg5(void)
{
    PyObject *magic_key = Py_BuildValue("s", "MAGIC");
    PyObject *magic_value = Py_BuildValue("y#", MAGIC, MAGIC_SIZE);
    PyObject *module = PyModule_Create(&module_definition);
    PyObject_SetAttr(module, magic_key, magic_value);
    return module;
}
