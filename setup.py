from distutils.core import setup, Extension

setup(ext_modules=[
    Extension('lib.tlg6', sources = ['ext/tlg6.c', 'ext/stream.c', 'ext/lzss.c']),
])
