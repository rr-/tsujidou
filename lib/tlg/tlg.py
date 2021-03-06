from typing import Tuple, Any
from lib.png import raw_to_png, png_to_raw
from lib.tlg import tlg0
from lib.tlg import tlg5
from lib.tlg import tlg6


def is_tlg(content: bytes) -> bool:
    return content.startswith((tlg0.MAGIC, tlg5.MAGIC, tlg6.MAGIC))


def tlg_to_png(content: bytes) -> Tuple[bytes, Any]:
    metadata = None
    if content.startswith(tlg0.MAGIC):
        width, height, raw_data, metadata = tlg0.decode_tlg_0(content)
    elif content.startswith(tlg5.MAGIC):
        width, height, raw_data = tlg5.decode_tlg_5(content)
    elif content.startswith(tlg6.MAGIC):
        width, height, raw_data = tlg6.decode_tlg_6(content)
    else:
        assert False, 'Not a TLG image'
    return raw_to_png(width, height, raw_data), metadata


def png_to_tlg(png_content: bytes, metadata: Any) -> bytes:
    width, height, raw_data = png_to_raw(png_content)
    return tlg0.encode_tlg_0(width, height, raw_data, metadata)
