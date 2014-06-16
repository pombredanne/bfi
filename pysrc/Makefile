LIB_DIR = ../src
TARGETS = bfi/_bfi.so

all: ${TARGETS}

bfi/_bfi.so: bfi/_bfi.c ${LIB_DIR}/libbfi.a
	rm -f $@
	python setup.py build_ext --inplace

test:
	python -m unittest discover

clean:
	rm -f ${TARGETS}
	rm -rf build
	rm -f *.pyc
