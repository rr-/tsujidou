#!/usr/bin/python3
import math
import struct
import zlib
from enum import IntEnum
from typing import Dict, Optional, List
from lib.crc64 import crc64
from lib.open_ext import ExtendedHandle, open_ext


GAME_TITLE = '辻堂さんの純愛ロード'
SCRIPT_HASH = crc64(GAME_TITLE.encode('sjis'))
ENTRY_COUNT_HASH = 0x26ACA46E


def get_file_name_hash(name: str) -> int:
    return crc64(name.encode('sjis'))


class FileType(IntEnum):
    PLAIN      = 0
    OBFUSCATED = 1
    COMPRESSED = 2


class FileEntry:
    def __init__(
            self,
            file_type: FileType,
            file_name_hash: int,
            file_name: Optional[str],
            offset: Optional[int],
            size_compressed: Optional[int],
            size_original: Optional[int],
            is_extractable: bool) -> None:
        self.file_type = file_type
        self.file_name_hash = file_name_hash
        self.file_name = file_name
        self.offset = offset
        self.size_compressed = size_compressed
        self.size_original = size_original
        self.is_extractable = is_extractable


class FileTable:
    def __init__(self, entries: List[FileEntry]) -> None:
        self.entries = entries


def read_file_table(
        handle: ExtendedHandle,
        file_name_hash_map: Dict[int, str]) -> FileTable:
    assert handle.tell() == 0
    entry_count = handle.read_u32_le() ^ ENTRY_COUNT_HASH
    return FileTable([
        read_file_entry(handle, file_name_hash_map)
        for i in range(entry_count)
    ])


def read_file_entry(
        handle: ExtendedHandle,
        file_name_hash_map: Dict[int, str]) -> FileEntry:
    file_name_hash  = handle.read_u64_le()
    file_type       = FileType(handle.read_u8() ^ (file_name_hash & 0xFF))
    offset          = handle.read_u32_le() ^ (file_name_hash & 0xFFFFFFFF)
    size_compressed = handle.read_u32_le() ^ (file_name_hash & 0xFFFFFFFF)
    size_original   = handle.read_u32_le() ^ (file_name_hash & 0xFFFFFFFF)

    file_name: Optional[str] = None
    if file_name_hash in file_name_hash_map:
        file_name = file_name_hash_map[file_name_hash]

    is_extractable = True
    if file_type != FileType.COMPRESSED:
        if file_name is not None:
            name = file_name.encode('sjis')
            offset          ^= name[len(name) >> 1]
            size_compressed ^= name[len(name) >> 2]
            size_original   ^= name[len(name) >> 3]
        else:
            is_extractable = False

    if not is_extractable:
        return FileEntry(
            file_type, file_name_hash, None, None, None, None, False)
    return FileEntry(
        file_type, file_name_hash, file_name,
        offset, size_compressed, size_original,
        True)


def read_file_content(handle: ExtendedHandle, entry: FileEntry) -> bytes:
    if entry.file_type == FileType.COMPRESSED:
        with handle.peek(entry.offset):
            content = handle.read(entry.size_compressed)
            content = _transform_script_content(content, entry.file_name_hash)
            return zlib.decompress(content)

    with handle.peek(entry.offset):
        content = handle.read(entry.size_compressed)
        if entry.file_type == FileType.OBFUSCATED:
            assert entry.file_name is not None
            assert entry.size_compressed is not None
            content = _transform_regular_content(
                content, entry.file_name, entry.size_compressed)
        return content


def _transform_script_content(content: bytes, content_hash: int) -> bytes:
    xor = (content_hash ^ SCRIPT_HASH) & 0xFFFFFFFF
    result = b''
    for _ in range(math.floor(len(content) / 4)):
        result += struct.pack('<I', struct.unpack('<I', content[0:4])[0] ^ xor)
        content = content[4:]
    result += content
    return result


def _transform_regular_content(
        content: bytes, file_name: str, limit: int) -> bytes:
    name = file_name.encode('sjis')
    block_len = math.floor(limit / len(name))
    content = bytearray(content)
    pos = 0
    for j in range(len(name) - 1):
        for k in range(block_len):
            content[pos] ^= name[j]
            pos += 1
    return content
