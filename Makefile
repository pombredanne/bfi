all:
	$(MAKE) -C src all
	$(MAKE) -C pysrc all

test:
	$(MAKE) -C pysrc test

clean:
	$(MAKE) -C src clean
	$(MAKE) -C pysrc clean
