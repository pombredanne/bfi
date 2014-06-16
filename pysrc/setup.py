from distutils.core import setup, Extension

setup(
    version='2.1',
    description='Bloom Filter Index',
    author='Tris Forster',
    url='https://github.com/tf198/bfi',
    packages=['bfi'],
    ext_modules=[Extension(
        "bfi/_bfi",
        sources=["bfi/_bfi.c"],
        include_dirs=['../src'],
        library_dirs=['../src'],
        libraries=['bfi']
    )],
)
