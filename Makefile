all:
	$(MAKE) -C src all
	$(MAKE) -C pysrc all

test:
	cd pysrc; python -m tests.simple; cd -

clean:
	$(MAKE) -C src clean
	$(MAKE) -C pysrc clean
