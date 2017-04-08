import io
from typing import Tuple, List
from lib.tlg import tlg5
from lib.tlg import tlg6
from lib.open_ext import ExtendedHandle


MAGIC = b'TLG0.0\x00sds\x1A'
Tags = List[Tuple[bytes, bytes]]


def extract_string(input: bytes) -> Tuple[bytes, bytes]:
    colon = input.index(b':')
    size = int(input[0:colon])
    input = input[colon+1:]
    return input[:size], input[size:]


def put_string(input: bytes) -> bytes:
    return str(len(input)).encode('ascii') + b':' + input


def decode_tlg_0(content: bytes) -> Tuple[int, int, bytes, Tags]:
    with ExtendedHandle(io.BytesIO(content)) as handle:
        assert handle.read(len(MAGIC)) == MAGIC

        sub_file_size = handle.read_u32_le()
        content = handle.read(sub_file_size)

        if content.startswith(tlg5.MAGIC):
            width, height, raw_data = tlg5.decode_tlg_5(content)
        elif content.startswith(tlg6.MAGIC):
            width, height, raw_data = tlg6.decode_tlg_6(content)
        else:
            assert False, 'Not a TLG image'

        tags = []  # type: Tags
        while True:
            chunk_name = handle.read(4)
            if not chunk_name:
                break
            chunk_size = handle.read_u32_le()
            chunk_data = handle.read(chunk_size)

            if chunk_name == b'tags':
                while chunk_data != b'':
                    key, chunk_data = extract_string(chunk_data)
                    assert chunk_data.startswith(b'='), 'Corrupt tags'
                    chunk_data = chunk_data[1:]

                    value, chunk_data = extract_string(chunk_data)
                    assert not chunk_data or chunk_data.startswith(b','), \
                        'Corrupt tags'
                    chunk_data = chunk_data[1:]

                    tags.append((key, value))
            else:
                raise NotImplementedError(
                    'Unknown chunk: {}'.format(chunk_name))

        return width, height, raw_data, tags


def encode_tlg_0(
        width: int, height: int, raw_data: bytes, tags: Tags) -> bytes:
    with ExtendedHandle(io.BytesIO(b'')) as handle:
        handle.write(MAGIC)
        sub_file_content = tlg5.encode_tlg_5(width, height, raw_data)
        handle.write_u32_le(len(sub_file_content))
        handle.write(sub_file_content)
        if tags:
            chunk_data = b','.join([
                put_string(key) + b'=' + put_string(value)
                for key, value in tags
            ])
            handle.write(b'tags')
            handle.write_u32_le(len(chunk_data))
            handle.write(chunk_data)
        return handle.getvalue()
