TARGETS = bfi libbfi.a

all: ${TARGETS}

bfi: bfi_tools.c libbfi.a
	gcc $< -L. -lbfi -o $@

libbfi.a: bfi.h bfi_v2.o murmur.o
	ar rcs $@ $^

%.o: %.c
	gcc -c -fPIC $< -o $@

clean:
	rm -f *.o *.a
	rm -f ${TARGETS}
