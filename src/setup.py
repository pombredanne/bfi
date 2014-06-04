from distutils.core import setup, Extension

setup(
    ext_modules=[Extension("_bfi", ["_bfi.c", "bfi_v2.c", "murmur.c"])],
)


