import io
import struct
from pathlib import Path
from typing import Any


class PeekObject:
    def __init__(self, handle: Any, *seek_args: Any) -> None:
        self._handle = handle
        self._seek_args = seek_args
        self._old_pos = 0

    def __enter__(self) -> None:
        self._old_pos = self._handle.tell()
        self._handle.seek(*self._seek_args)

    def __exit__(self, *unused: Any) -> None:
        self._handle.seek(self._old_pos)


class ExtendedHandle:
    def __init__(self, handle: Any) -> None:
        self._handle = handle

    def __getattr__(self, attr: Any) -> Any:
        return getattr(self._handle, attr)

    def __enter__(self) -> 'ExtendedHandle':
        return self

    def __exit__(self, *unused: Any) -> None:
        self._handle.close()

    def skip(self, how_many: int) -> None:
        self._handle.seek(how_many, io.SEEK_CUR)

    def peek(self, *args: Any) -> PeekObject:
        return PeekObject(self._handle, *args)

    def read_until_zero(self) -> bytes:
        ret = b''
        byte = self._handle.read(1)
        while byte != b"\x00":
            ret += byte
            byte = self._handle.read(1)
        return ret

    def read_until_end(self) -> bytes:
        return self._handle.read()

    def read_u8(self) -> int:
        return struct.unpack('B', self._handle.read(1))[0]

    def read_u16_le(self) -> int:
        return struct.unpack('<H', self._handle.read(2))[0]

    def read_u32_le(self) -> int:
        return struct.unpack('<I', self._handle.read(4))[0]

    def read_u64_le(self) -> int:
        return struct.unpack('<Q', self._handle.read(8))[0]

    def read_u16_be(self) -> int:
        return struct.unpack('>H', self._handle.read(2))[0]

    def read_u32_be(self) -> int:
        return struct.unpack('>I', self._handle.read(4))[0]

    def read_u64_be(self) -> int:
        return struct.unpack('>Q', self._handle.read(8))[0]

    def write_u8(self, num: int) -> None:
        self._handle.write(struct.pack('B', num))

    def write_u16_le(self, num: int) -> None:
        self._handle.write(struct.pack('<H', num))

    def write_u32_le(self, num: int) -> None:
        self._handle.write(struct.pack('<I', num))

    def write_u64_le(self, num: int) -> None:
        self._handle.write(struct.pack('<Q', num))

    def write_u16_be(self, num: int) -> None:
        self._handle.write(struct.pack('>H', num))

    def write_u32_be(self, num: int) -> None:
        self._handle.write(struct.pack('>I', num))

    def write_u64_be(self, num: int) -> None:
        self._handle.write(struct.pack('>Q', num))


def open_ext(path: Path, mode: str) -> ExtendedHandle:
    return ExtendedHandle(open(path, mode))
