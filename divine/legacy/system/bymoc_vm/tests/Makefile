# ByMoC VM - Bytecode based Model Checker Virtual Machine
# version 0.9.3 date 2005-06-23
# Copyright (C) 2005: Stefan Schuermans <stes@i2.informatik.rwth-aachen.de>
#                     Lehrstuhl fuer Informatik II, RWTH Aachen
# Copyleft: GNU public license - http://www.gnu.org/copyleft/gpl.html

PERL=perl
BYMOC_ASM=$(PERL) -I.. ../bymoc_asm.pl
BYMOC_VM=../bymoc_vm
EXAMPLES = benchmark.b benchmark2.b benchmark3.b benchmark4.b benchmark5.b example.b mutex.b

.phony: default help build example benchmark clean

default: help build

help:
	@echo -e "make targets:\n" \
	         "  build       compile and assemble examples\n" \
	         "  example     interactive simulation using example code\n" \
	         "  benchmark   small benchmark using depth-first search\n" \
	         "  clean       remove all generated files\n" \

build: $(EXAMPLES)

%.b: %.s
	$(BYMOC_ASM) $<

example: example.b
	$(BYMOC_VM) example.b

benchmark: benchmark.b benchmark2.b benchmark3.b benchmark4.b benchmark5.b
	$(BYMOC_VM) -D 100000 benchmark.b
	$(BYMOC_VM) -D 100000 benchmark2.b
	$(BYMOC_VM) -D 100000 benchmark3.b
	$(BYMOC_VM) -D 16 benchmark4.b
	$(BYMOC_VM) -D 7000 benchmark5.b

clean:
	rm -f *.b *.l 
