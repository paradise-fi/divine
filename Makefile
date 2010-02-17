all check divine-mc clean lcov-report install:
	$(MAKE) -C _build $(@)
distclean:
	rm -rf _build
