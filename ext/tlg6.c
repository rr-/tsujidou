#include <Python.h>
#include <string.h>
#include "stream.h"
#include "lzss.h"
#include "pixel.h"

#define MAGIC "TLG6.0\x00raw\x1A"
#define MAGIC_SIZE 11

#define W_BLOCK_SIZE 8
#define H_BLOCK_SIZE 8
#define GOLOMB_N_COUNT 4
#define LEADING_ZERO_TABLE_BITS 12
#define LEADING_ZERO_TABLE_SIZE (1 << LEADING_ZERO_TABLE_BITS)

static uint8_t leading_zero_table[LEADING_ZERO_TABLE_SIZE];
static uint8_t golomb_bit_size_table[GOLOMB_N_COUNT * 2 * 128][GOLOMB_N_COUNT];

typedef struct
{
    uint8_t channel_count;
    uint8_t data_flags;
    uint8_t color_type;
    uint8_t external_golomb_table;
    uint32_t image_width;
    uint32_t image_height;
    uint32_t max_bit_size;
    size_t x_block_count;
    size_t y_block_count;
} Tlg6Header;

typedef struct
{
    unsigned char *data;
    size_t data_size;
} Tlg6FilterTypes;

static inline uint32_t tlg6_pixel_to_bgra(const Pixel *p)
{
    return p->b | (p->g << 8) | (p->r << 16) | (p->a << 24);
}

static void tlg6_transformer0(Pixel *p)
{
}

static void tlg6_transformer1(Pixel *p)
{
    p->r += p->g;
    p->b += p->g;
}

static void tlg6_transformer2(Pixel *p)
{
    p->g += p->b;
    p->r += p->g;
}

static void tlg6_transformer3(Pixel *p)
{
    p->g += p->r;
    p->b += p->g;
}

static void tlg6_transformer4(Pixel *p)
{
    p->b += p->r;
    p->g += p->b;
    p->r += p->g;
}

static void tlg6_transformer5(Pixel *p)
{
    p->b += p->r;
    p->g += p->b;
}

static void tlg6_transformer6(Pixel *p)
{
    p->b += p->g;
}

static void tlg6_transformer7(Pixel *p)
{
    p->g += p->b;
}

static void tlg6_transformer8(Pixel *p)
{
    p->r += p->g;
}

static void tlg6_transformer9(Pixel *p)
{
    p->r += p->b;
    p->g += p->r;
    p->b += p->g;
}

static void tlg6_transformerA(Pixel *p)
{
    p->b += p->r;
    p->g += p->r;
}

static void tlg6_transformerB(Pixel *p)
{
    p->r += p->b;
    p->g += p->b;
}

static void tlg6_transformerC(Pixel *p)
{
    p->r += p->b;
    p->g += p->r;
}

static void tlg6_transformerD(Pixel *p)
{
    p->b += p->g;
    p->r += p->b;
    p->g += p->r;
}

static void tlg6_transformerE(Pixel *p)
{
    p->g += p->r;
    p->b += p->g;
    p->r += p->b;
}

static void tlg6_transformerF(Pixel *p)
{
    p->g += p->b << 1;
    p->r += p->b << 1;
}

static void (*tlg6_transformers[16])(Pixel *p) =
{
    &tlg6_transformer0, &tlg6_transformer1,
    &tlg6_transformer2, &tlg6_transformer3,
    &tlg6_transformer4, &tlg6_transformer5,
    &tlg6_transformer6, &tlg6_transformer7,
    &tlg6_transformer8, &tlg6_transformer9,
    &tlg6_transformerA, &tlg6_transformerB,
    &tlg6_transformerC, &tlg6_transformerD,
    &tlg6_transformerE, &tlg6_transformerF,
};

static inline uint32_t make_gt_mask(const uint32_t a, const uint32_t b)
{
    const uint32_t tmp2 = ~b;
    const uint32_t tmp =
        ((a & tmp2) + (((a ^ tmp2) >> 1) & 0x7F7F7F7F)) & 0x80808080;
    return ((tmp >> 7) + 0x7F7F7F7F) ^ 0x7F7F7F7F;
}

static inline uint32_t packed_bytes_add(
    const uint32_t a, const uint32_t b)
{
    return a + b - ((((a & b) << 1) + ((a ^ b) & 0xFEFEFEFE)) & 0x01010100);
}

static inline uint32_t tlg6_filter_med(
    const uint32_t a, const uint32_t b, const uint32_t c, const uint32_t v)
{
    const uint32_t aa_gt_bb = make_gt_mask(a, b);
    const uint32_t a_xor_b_and_aa_gt_bb = ((a ^ b) & aa_gt_bb);
    const uint32_t aa = a_xor_b_and_aa_gt_bb ^ a;
    const uint32_t bb = a_xor_b_and_aa_gt_bb ^ b;
    const uint32_t n = make_gt_mask(c, bb);
    const uint32_t nn = make_gt_mask(aa, c);
    const uint32_t m = ~(n | nn);
    return packed_bytes_add(
        (n & aa) | (nn & bb) | ((bb & m) - (c & m) + (aa & m)), v);
}

static inline uint32_t tlg6_filter_avg(
    const uint32_t a, const uint32_t b, const uint32_t c, const uint32_t v)
{
    return packed_bytes_add((a & b)
        + (((a ^ b) & 0xFEFEFEFE) >> 1)
        + ((a ^ b) & 0x01010101), v);
}

static uint32_t (*tlg6_filters[2])(
    const uint32_t, const uint32_t, const uint32_t, const uint32_t) =
{
    &tlg6_filter_med,
    &tlg6_filter_avg,
};

static void tlg6_init_tables(void)
{
    short golomb_compression_table[GOLOMB_N_COUNT][9] =
    {
        {3, 7, 15, 27, 63, 108, 223, 448, 130},
        {3, 5, 13, 24, 51, 95, 192, 384, 257},
        {2, 5, 12, 21, 39, 86, 155, 320, 384},
        {2, 3, 9, 18, 33, 61, 129, 258, 511},
    };

    for (int i = 0; i < LEADING_ZERO_TABLE_SIZE; i++)
    {
        int cnt = 0;
        int j = 1;

        while (j != LEADING_ZERO_TABLE_SIZE && !(i & j))
        {
            j <<= 1;
            cnt++;
        }

        cnt++;

        if (j == LEADING_ZERO_TABLE_SIZE)
            cnt = 0;

        leading_zero_table[i] = cnt;
    }

    for (int n = 0; n < GOLOMB_N_COUNT; n++)
    {
        int a = 0;
        for (int i = 0; i < 9; i++)
        {
            for (int j = 0; j < golomb_compression_table[n][i]; j++)
                golomb_bit_size_table[a++][n] = i;
        }
    }
}

static Tlg6FilterTypes *tlg6_ft_create(void)
{
    Tlg6FilterTypes *ft = PyMem_RawMalloc(sizeof(Tlg6FilterTypes));
    if (!ft)
    {
        PyErr_SetNone(PyExc_MemoryError);
        return NULL;
    }
    ft->data = NULL;
    ft->data_size = 0;
    return ft;
}

static int tlg6_ft_read(
    Tlg6FilterTypes *ft, Stream *stream, const Tlg6Header *header)
{
    assert(ft);
    assert(stream);
    assert(header);
    unsigned char *data_comp = NULL;
    int ret = 0;

    const size_t data_orig_size =
        header->x_block_count * header->y_block_count;

    uint32_t data_comp_size = 0;
    if (!stream_read_u32_le(stream, &data_comp_size))
        goto end;

    data_comp = PyMem_RawMalloc(data_comp_size);
    if (!data_comp)
    {
        PyErr_SetString(PyExc_ValueError, "Reading beyond EOF");
        goto end;
    }
    if (!stream_read_data(stream, data_comp, data_comp_size))
        goto end;

    unsigned char dict[4096] = {0};
    unsigned char *dict_ptr = dict;
    for (int i = 0; i < 32; i++)
    {
        for (int j = 0; j < 16; j++)
        {
            for (int k = 0; k < 4; k++) *dict_ptr++ = i;
            for (int k = 0; k < 4; k++) *dict_ptr++ = j;
        }
    }
    size_t dict_pos = 0;

    unsigned char *data_orig = lzss_decompress(
        data_comp, data_comp_size, data_orig_size, dict, &dict_pos);

    if (!data_orig)
        goto end;

    ft->data = data_orig;
    ft->data_size = data_orig_size;
    ret = 1;
end:
    if (data_comp)
        PyMem_RawFree(data_comp);
    return ret;
}

static void tlg6_ft_destroy(Tlg6FilterTypes *ft)
{
    assert(ft);
    if (ft->data)
        PyMem_RawFree(ft->data);
    PyMem_RawFree(ft);
}

static int tlg6_header_read(Stream *stream, Tlg6Header *header)
{
    assert(stream);
    assert(header);
    if (!stream_read_u8(stream, &header->channel_count)) return 0;
    if (!stream_read_u8(stream, &header->data_flags)) return 0;
    if (!stream_read_u8(stream, &header->color_type)) return 0;
    if (!stream_read_u8(stream, &header->external_golomb_table)) return 0;
    if (!stream_read_u32_le(stream, &header->image_width)) return 0;
    if (!stream_read_u32_le(stream, &header->image_height)) return 0;
    if (!stream_read_u32_le(stream, &header->max_bit_size)) return 0;
    header->x_block_count = ((header->image_width - 1) / W_BLOCK_SIZE) + 1;
    header->y_block_count = ((header->image_height - 1) / H_BLOCK_SIZE) + 1;
    return 1;
}

static void tlg6_decode_golomb_values(
    uint8_t *block_data, const int pixel_count, uint8_t *bit_pool)
{
    assert(block_data);
    assert(bit_pool);

    int n = GOLOMB_N_COUNT - 1;
    int a = 0;

    int bit_pos = 1;
    uint8_t zero = (*bit_pool & 1) ? 0 : 1;
    uint8_t *limit = block_data + pixel_count * 4;

    while (block_data < limit)
    {
        int count;
        {
            uint32_t t = *((uint32_t*)bit_pool) >> bit_pos;
            int b = leading_zero_table[t & (LEADING_ZERO_TABLE_SIZE - 1)];
            int bit_count = b;
            while (!b)
            {
                bit_count += LEADING_ZERO_TABLE_BITS;
                bit_pos += LEADING_ZERO_TABLE_BITS;
                bit_pool += bit_pos >> 3;
                bit_pos &= 7;
                t = *((uint32_t*)bit_pool) >> bit_pos;
                b = leading_zero_table[t & (LEADING_ZERO_TABLE_SIZE - 1)];
                bit_count += b;
            }

            bit_pos += b;
            bit_pool += bit_pos >> 3;
            bit_pos &= 7;

            bit_count--;
            count = 1 << bit_count;
            t = *((uint32_t*)bit_pool) >> bit_pos;
            count += (t & (count - 1));

            bit_pos += bit_count;
            bit_pool += bit_pos >> 3;
            bit_pos &= 7;
        }

        if (zero)
        {
            do
            {
                *block_data = 0;
                block_data += 4;
            }
            while (--count && block_data < limit);
        }
        else
        {
            do
            {
                int bit_count;
                int b;

                uint32_t t = *((uint32_t*)bit_pool) >> bit_pos;
                if (t)
                {
                    b = leading_zero_table[
                        t & (LEADING_ZERO_TABLE_SIZE - 1)];
                    bit_count = b;
                    while (!b)
                    {
                        bit_count += LEADING_ZERO_TABLE_BITS;
                        bit_pos += LEADING_ZERO_TABLE_BITS;
                        bit_pool += bit_pos >> 3;
                        bit_pos &= 7;
                        t = *((uint32_t*)bit_pool) >> bit_pos;
                        b = leading_zero_table[
                            t & (LEADING_ZERO_TABLE_SIZE - 1)];
                        bit_count += b;
                    }
                    bit_count--;
                }
                else
                {
                    bit_pool += 5;
                    bit_count = bit_pool[-1];
                    bit_pos = 0;
                    t = *((uint32_t*)bit_pool);
                    b = 0;
                }

                if (a >= GOLOMB_N_COUNT * 2 * 128) a = 0;
                if (n >= GOLOMB_N_COUNT) n = 0;

                int k = golomb_bit_size_table[a][n];
                int v = (bit_count << k) + ((t >> b) & ((1 << k) - 1));
                int sign = (v & 1) - 1;
                v >>= 1;
                a += v;

                *((uint8_t*)block_data) = ((v ^ sign) + sign + 1);
                block_data += 4;

                bit_pos += b;
                bit_pos += k;
                bit_pool += bit_pos >> 3;
                bit_pos &= 7;

                if (--n < 0)
                {
                    a >>= 1;
                    n = GOLOMB_N_COUNT - 1;
                }
            }
            while (--count && block_data < limit);
        }

        zero ^= 1;
    }
}

static void tlg6_decode_line(
    Pixel *prev_line,
    Pixel *current_line,
    int start_block,
    int block_limit,
    uint8_t *filter_types,
    int skip_block_bytes,
    uint32_t *in,
    int odd_skip,
    int dir,
    const Tlg6Header *header)
{
    assert(prev_line);
    assert(current_line);
    assert(filter_types);
    assert(in);
    assert(header);

    Pixel left, top_left;
    int step;

    if (start_block)
    {
        prev_line += start_block * W_BLOCK_SIZE;
        current_line += start_block * W_BLOCK_SIZE;
        left = current_line[-1];
        top_left = prev_line[-1];
    }
    else
    {
        left.r = left.g = left.b = 0;
        top_left.r = top_left.g = top_left.b = 0;
        left.a = top_left.a = header->channel_count == 3 ? 0xFF : 0;
    }

    in += skip_block_bytes * start_block;
    step = (dir & 1) ? 1 : -1;

    for (int i = start_block; i < block_limit; i++)
    {
        int w = header->image_width - i * W_BLOCK_SIZE;
        if (w > W_BLOCK_SIZE)
            w = W_BLOCK_SIZE;

        int ww = w;
        if (step == -1)
            in += ww - 1;

        if (i & 1)
            in += odd_skip * ww;

        uint32_t (*filter)(
                const uint32_t,
                const uint32_t,
                const uint32_t,
                const uint32_t) =
            tlg6_filters[filter_types[i] & 1];
        void (*transformer)(Pixel *) = tlg6_transformers[filter_types[i] >> 1];

        do
        {
            Pixel inn;
            inn.a = *in >> 24;
            inn.r = *in >> 16;
            inn.g = *in >> 8;
            inn.b = *in;
            transformer(&inn);

            Pixel top = *prev_line;
            uint32_t result = filter(
                tlg6_pixel_to_bgra(&left),
                tlg6_pixel_to_bgra(&top),
                tlg6_pixel_to_bgra(&top_left),
                tlg6_pixel_to_bgra(&inn));
            left.a = result >> 24;
            left.r = result >> 16;
            left.g = result >> 8;
            left.b = result;
            if (header->channel_count == 3)
                left.a = 0xFF;

            top_left = top;
            *current_line++ = left;

            prev_line++;
            in += step;
        }
        while (--w);

        in += skip_block_bytes + (step == 1 ? - ww : 1);
        if (i & 1)
            in -= odd_skip * ww;
    }
}

static PyObject *tlg6_decode(PyObject *self, PyObject *args)
{
    Stream *stream = NULL;
    Tlg6FilterTypes *ft = NULL;
    Pixel *block_data = NULL;
    Pixel *zero_line = NULL;
    Pixel *prev_line = NULL;
    Pixel *image_data = NULL;
    Py_buffer input = {0};
    PyObject *output = NULL;
    Tlg6Header header;

    if (!PyArg_ParseTuple(args, "y*", &input))
        goto end;

    stream = stream_create(input.buf, input.len);
    if (!stream)
        goto end;

    if (memcmp(stream->data, MAGIC, MAGIC_SIZE))
    {
        PyErr_SetString(PyExc_ValueError, "Not a TLG6 image");
        goto end;
    }
    stream->pos += MAGIC_SIZE;

    if (!tlg6_header_read(stream, &header))
        goto end;
    if (header.channel_count != 3 && header.channel_count != 4)
    {
        PyErr_SetString(PyExc_ValueError, "Unsupported channel count");
        goto end;
    }

    ft = tlg6_ft_create();
    if (!ft)
        goto end;
    if (!tlg6_ft_read(ft, stream, &header))
        goto end;

    const size_t image_data_size =
        header.image_height * header.image_width * 4;
    image_data = PyMem_RawMalloc(image_data_size);
    if (!image_data)
    {
        PyErr_SetNone(PyExc_MemoryError);
        goto end;
    }

    block_data = PyMem_RawMalloc(4 * header.image_width * H_BLOCK_SIZE);
    if (!block_data)
    {
        PyErr_SetNone(PyExc_MemoryError);
        goto end;
    }
    zero_line = PyMem_RawMalloc(4 * header.image_width);
    if (!zero_line)
    {
        PyErr_SetNone(PyExc_MemoryError);
        goto end;
    }
    memset(zero_line, 0, 4 * header.image_width);
    prev_line = zero_line;

    uint32_t main_count = header.image_width / W_BLOCK_SIZE;
    for (size_t y = 0; y < header.image_height; y += H_BLOCK_SIZE)
    {
        size_t ylim = y + H_BLOCK_SIZE;
        if (ylim >= header.image_height)
            ylim = header.image_height;

        int pixel_count = (ylim - y) * header.image_width;
        for (int c = 0; c < header.channel_count; c++)
        {
            uint32_t bit_size = 0;
            if (!stream_read_u32_le(stream, &bit_size))
                goto end;

            int method = (bit_size >> 30) & 3;
            if (method != 0)
            {
                PyErr_SetString(
                    PyExc_NotImplementedError, "Unsupported encoding method");
                goto end;
            }

            int byte_size = ((bit_size & 0x3FFFFFFF) + 7) / 8;

            // Although decode_golomb_values accesses only valid bits, it casts
            // to uint32_t* which might access bits out of bounds. This is to
            // make sure those calls don't cause access violations.
            unsigned char *bit_pool = PyMem_RawMalloc(byte_size + 4);
            if (!bit_pool)
            {
                PyErr_SetNone(PyExc_MemoryError);
                goto end;
            }
            if (!stream_read_data(stream, bit_pool, byte_size))
            {
                PyMem_RawFree(bit_pool);
                goto end;
            }

            tlg6_decode_golomb_values(
                ((uint8_t*)block_data) + c, pixel_count, bit_pool);

            PyMem_RawFree(bit_pool);
        }

        uint8_t *ft_data =
            ft->data + (y / H_BLOCK_SIZE) * header.x_block_count;
        int skip_bytes = (ylim - y) * W_BLOCK_SIZE;

        for (size_t yy = y; yy < ylim; yy++)
        {
            Pixel *current_line = image_data + yy * header.image_width;
            int dir = (yy & 1) ^ 1;
            int odd_skip = ((ylim - yy -1) - (yy - y));

            if (main_count)
            {
                int start = ((header.image_width < W_BLOCK_SIZE)
                    ? header.image_width
                    : W_BLOCK_SIZE) * (yy - y);

                tlg6_decode_line(
                    prev_line,
                    current_line,
                    0,
                    main_count,
                    ft_data,
                    skip_bytes,
                    ((uint32_t*)block_data) + start,
                    odd_skip,
                    dir,
                    &header);
            }

            if (main_count != header.x_block_count)
            {
                int ww = header.image_width - main_count * W_BLOCK_SIZE;
                if (ww > W_BLOCK_SIZE)
                    ww = W_BLOCK_SIZE;

                int start = ww * (yy - y);
                tlg6_decode_line(
                    prev_line,
                    current_line,
                    main_count,
                    header.x_block_count,
                    ft_data,
                    skip_bytes,
                    ((uint32_t*)block_data) + start,
                    odd_skip,
                    dir,
                    &header);
            }

            prev_line = current_line;
        }
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
    if (image_data) PyMem_RawFree(image_data);
    if (block_data) PyMem_RawFree(block_data);
    if (zero_line) PyMem_RawFree(zero_line);
    if (stream) stream_destroy(stream);
    if (ft) tlg6_ft_destroy(ft);
    PyBuffer_Release(&input);
    return output;
}

static PyMethodDef Methods[] = {
    {"decode_tlg_6", tlg6_decode, METH_VARARGS, "Decode a tlg6 image"},
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef module_definition = {
   PyModuleDef_HEAD_INIT, "lib.tlg.tlg6", NULL, -1, Methods,
};

PyMODINIT_FUNC PyInit_tlg6(void)
{
    tlg6_init_tables();
    PyObject *magic_key = Py_BuildValue("s", "MAGIC");
    PyObject *magic_value = Py_BuildValue("y#", MAGIC, MAGIC_SIZE);
    PyObject *module = PyModule_Create(&module_definition);
    PyObject_SetAttr(module, magic_key, magic_value);
    return module;
}
