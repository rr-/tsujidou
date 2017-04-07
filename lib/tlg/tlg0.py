MAGIC = b'TLG0.0\x00sds\x1A'


def decode_tlg_0(content: bytes):
    assert content.startswith(MAGIC)
    raise NotImplementedError('Not implemented')
