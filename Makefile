all:
	$(MAKE) -C src all
	$(MAKE) -C pysrc all

clean:
	$(MAKE) -C src clean
	$(MAKE) -C pysrc clean
