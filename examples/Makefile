.PHONY: all clean
.SUFFIXES:

HC=ghc
CC=gcc
HCFLAGS=-O2 #-threaded
#HCFLAGS=-O2 -prof -auto-all -caf-all
#HCFLAGS=-O2 -funbox-strict-fields -fvia-C -optc-O2
DRIFT=DrIFT

all: BenchmarkHs.so BenchmarkC.so

BenchmarkHs.o BenchmarkHs.so: Divine/Pool.hs Divine/Generator.hs Divine/Blob.hs Divine/Circular.hs Divine/module_init.c ../divine/pool.ffi.h

# We should use -fPIC here, but GHC manual says it doesn't quite work on x86
# Linux yet (5.10.6).

%.o: %.hs
	$(HC) $(HCFLAGS) --make -c $<

%.so: %.hs %.o
	$(HC) $(HCFLAGS) --make \
		-no-hs-main -optl '-shared' -optl "-L." \
		-optl '-ldivine' \
		-optc '-I.' -optc '-I..' -optc '-DMODULE='$(*F) \
		-o $@ $< Divine/module_init.c

%.hs: %.in.hs
	$(DRIFT) $< >$@

%.so: %.c
	$(CC) -shared -o $(@) $(<)

clean:
	$(RM) $(wildcard *.o *.hi *.so *_stub.c *_stub.h)
	$(RM) $(wildcard Divine/*.o Divine/*.hi Divine/*.so Divine/*_stub.c Divine/*_stub.h)
