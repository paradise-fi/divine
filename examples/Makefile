.PHONY: all clean
.SUFFIXES:

HC=ghc
CC=gcc
HCFLAGS=-O2 #-threaded
#HCFLAGS=-O2 -prof -auto-all -caf-all
#HCFLAGS=-O2 -funbox-strict-fields -fvia-C -optc-O2
DRIFT=DrIFT

all: BenchmarkHs.so BenchmarkC.so

BenchmarkHs.o: Divine/Generator.hs
BenchmarkHs.so: Divine/Generator.hs

# We should use -fPIC here, but GHC manual says it doesn't quite work on x86
# Linux yet (5.10.6).

%.o: %.hs
	$(HC) $(HCFLAGS) --make -c $<

%.so: %.hs %.o
	$(HC) $(HCFLAGS) --make \
		-no-hs-main -optl '-shared' -optc '-I.' -optc '-DMODULE='$(*F) \
		-o $@ $< Divine/module_init.c

%.hs: %.in.hs
	$(DRIFT) $< >$@

%.so: %.c
	$(CC) -shared -o $(@) $(<)

clean:
	$(RM) $(wildcard *.o *.hi *.so *_stub.c *_stub.h)
