TARGETS = bfi libbfi.a

all: ${TARGETS}

bfi: bfi_tools.c libbfi.a
	gcc $< -L. -lbfi -o $@

libbfi.a: bfi.h bfi.o murmur.o
	ar rcs $@ $^

%.o: %.c
	gcc -c -fPIC $< -o $@

test: bfi
	rm -f test.db
	./bfi append test.db foo bar
	./bfi append test.db tar nar
	./bfi lookup test.db tar

clean:
	rm -f *.o *.a
	rm -f ${TARGETS}
