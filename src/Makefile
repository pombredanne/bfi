
all: bfi_v1 bfi_v2

bfi_%: bfi_tools.c bfi_%.c murmur.c
	gcc $^ -o $@