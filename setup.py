from distutils.core import setup, Extension

setup(ext_modules=[
    Extension('lib.tlg.tlg5', sources=['ext/tlg5.c', 'ext/stream.c', 'ext/lzss.c']),
    Extension('lib.tlg.tlg6', sources=['ext/tlg6.c', 'ext/stream.c', 'ext/lzss.c']),
])
