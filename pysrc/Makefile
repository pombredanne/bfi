LIB_DIR = ../src
TARGETS = _bfi.so

all: ${TARGETS}

_bfi.so: _bfi.c ${LIB_DIR}/libbfi.a
	python setup.py build_ext --inplace

clean:
	rm -f ${TARGETS}
	rm -rf build
	rm -f *.pyc
