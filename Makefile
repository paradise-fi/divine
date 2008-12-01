all check divine-mc clean lcov-report:
	$(MAKE) -C _build $(@)
install:
	$(MAKE) -C _build/tools install
distclean:
	rm -rf _build
