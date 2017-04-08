def decode_script(content: bytes) -> bytes:
    return content.decode('cp932').encode('utf-8')


def encode_script(content: bytes) -> bytes:
    return content.decode('utf-8').encode('cp932')
