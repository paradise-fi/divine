all check divine clean lcov-report install unit functional manual website:
	$(MAKE) -C _build $(@)
dist:
	$(MAKE) -C _build package_source
	cp _build/divine-*.tar.gz .

distclean:
	rm -rf _build

.PHONY: all check divine clean lcov-report install unit functional manual website
