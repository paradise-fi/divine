all check divine-mc clean:
	$(MAKE) -C _build $(@)
install:
	$(MAKE) -C _build/tools install
