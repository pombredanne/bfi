
all: bfi_v1 bfi_v2 _bfi.so

bfi_%: bfi_tools.c bfi_%.c murmur.c
	gcc $^ -o $@

_bfi.so: _bfi.c bfi_v2.c murmur.c
	python setup.py build_ext --inplace