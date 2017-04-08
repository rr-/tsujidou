import io
import PIL.Image


def raw_to_png(width: int, height: int, raw_data: bytes) -> bytes:
    image = PIL.Image.frombytes(
        mode='RGBA', size=(width, height), data=raw_data)
    with io.BytesIO() as handle:
        image.save(handle, format='png')
        return handle.getvalue()


def png_to_raw(png_content: bytes) -> bytes:
    with io.BytesIO(bytes(png_content)) as handle:
        image = PIL.Image.open(handle)
        return (
            image.width,
            image.height,
            b''.join(bytes(b) for b in image.getdata()))
