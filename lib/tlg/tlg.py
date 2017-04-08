import io
import PIL.Image
from lib.tlg import tlg0
from lib.tlg import tlg5
from lib.tlg import tlg6


def is_tlg(content: bytes) -> bool:
    return content.startswith((tlg0.MAGIC, tlg5.MAGIC, tlg6.MAGIC))


def tlg_to_png(content: bytes) -> bytes:
    if content.startswith(tlg0.MAGIC):
        return tlg0.decode_tlg_0(content)

    if content.startswith(tlg5.MAGIC):
        width, height, data = tlg5.decode_tlg_5(content)
    elif content.startswith(tlg6.MAGIC):
        width, height, data = tlg6.decode_tlg_6(content)
    else:
        assert False, 'Not a TLG image'

    image = PIL.Image.frombytes(
        mode='RGBA', size=(width, height), data=data)
    with io.BytesIO() as handle:
        image.save(handle, format='png')
        return handle.getvalue()
