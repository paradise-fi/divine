all check divine clean lcov-report install:
	$(MAKE) -C _build $(@)
distclean:
	rm -rf _build

.PHONY: all check divine clean lcov-report install
