from distutils.core import setup, Extension

setup(
    ext_modules=[Extension("_bfi", ["_bfi.c", "../src/bfi_v2.c", "../src/murmur.c"])],
)
