GENERATOR != if ninja --version > /dev/null 2>&1 || \
                ninja-build --version > /dev/null 2>&1; then echo Ninja; else echo Unix Makefiles; fi
# fallback
GENERATOR ?= Unix Makefiles

-include /etc/divine.make # for system-wide config
-include local.make

CMAKE ?= cmake
CC ?= cc
CXX ?= c++
DEFAULT_FLAVOUR ?= release
PREFIX ?= /opt/divine

MAKEFLAGS ?= --no-print-directory
OBJ ?= $(PWD)/_build.
BENCH_NAME ?= $(LOGNAME)
EXTRA != if test "$(GENERATOR)" = Ninja && test -n "$(VERBOSE)"; then echo -v -d explain; fi; \
         if test -n "$(JOBS)"; then echo -j $(JOBS); fi

TOOLDIR = $(OBJ)toolchain
CLANG = $(TOOLDIR)/clang/
RTBIN = $(TOOLDIR)/runtime
RTSRC = $(PWD)/runtime

LIBUNWIND_LDIR = $(RTBIN)/libunwind/src
CXX_LDIR = $(TOOLDIR)/lib
CXX_STATIC = $(TOOLDIR)/lib-static

LDFLAGS_ = -L$(LIBUNWIND_LDIR) -Wl,-rpath,$(LIBUNWIND_LDIR) \
           -L$(CXX_LDIR) -Wl,-rpath,$(CXX_LDIR)
BUILD_RPATH = $(LIBUNWIND_LDIR):$(CXX_LDIR)

CXXFLAGS_ = -isystem $(RTSRC)/libcxxabi/include -isystem $(RTSRC)/libcxx/include \
            -isystem $(RTSRC)/libunwind/include \
            -stdlib=libc++ -nostdinc++ -Wno-unused-command-line-argument

TOOLCHAIN_ = -DCMAKE_C_COMPILER=$(CLANG)/bin/clang \
	     -DCMAKE_CXX_COMPILER=$(CLANG)/bin/clang++ \
	     -DCMAKE_CXX_FLAGS="$(CXXFLAGS_)"
TOOLCHAIN  ?= $(TOOLCHAIN_) \
	      -DCMAKE_EXE_LINKER_FLAGS="$(LDFLAGS_)" -DCMAKE_SHARED_LINKER_FLAGS="$(LDFLAGS_)"
TOOLSTAMP  ?= $(TOOLDIR)/stamp-v2

CONFIG        += -DCMAKE_INSTALL_PREFIX=${PREFIX} -DBUILD_SHARED_LIBS=ON
static_FLAGS  += -DCMAKE_BUILD_TYPE=Release $(TOOLCHAIN) $(CONFIG) \
                 -DBUILD_SHARED_LIBS=OFF -DSTATIC_BUILD=ON
bench_FLAGS   += -DCMAKE_BUILD_TYPE=Release $(TOOLCHAIN) -DBUILD_SHARED_LIBS=OFF \
		 -DCMAKE_EXE_LINKER_FLAGS="-L$(CXX_STATIC) $(LDFLAGS_)"
release_FLAGS += -DCMAKE_BUILD_TYPE=RelWithDebInfo $(TOOLCHAIN) $(CONFIG)
semidbg_FLAGS += -DCMAKE_BUILD_TYPE=SemiDbg $(TOOLCHAIN) $(CONFIG)
debug_FLAGS   += -DCMAKE_BUILD_TYPE=Debug $(TOOLCHAIN) $(CONFIG)

asan_CXXFLAGS += -fsanitize=address -fno-omit-frame-pointer -fno-optimize-sibling-calls -g -O1
asan_FLAGS = $(debug_FLAGS) -DCMAKE_CXX_FLAGS_DEBUG="$(asan_CXXFLAGS)"

toolchain_FLAGS += -DCMAKE_BUILD_TYPE=RelWithDebInfo -DTOOLCHAIN=ON \
		   -DCMAKE_CXX_COMPILER=$(CXX) -DCMAKE_C_COMPILER=$(CC) \
		   -DCMAKE_INSTALL_PREFIX=${PREFIX}

all: $(DEFAULT_FLAVOUR)

FLAVOURS = debug asan release semidbg static bench
TARGETS = divine unit functional website check llvm-utils clang test-divine \
          install lart runner divbench divcheck divcc
DEFTARGETS = divine unit functional website check install lart divcheck divcc

${DEFTARGETS}:
	$(MAKE) $(DEFAULT_FLAVOUR)-$@

${FLAVOURS}:
	$(MAKE) $@-divine

divbench:
	$(MAKE) bench-divbench
	@echo your binary is at $(OBJ)bench/tools/divbench

divbench-install:
	test -d $(BENCH_DIR)
	$(MAKE) bench-divbench
	cp $(OBJ)bench/tools/divbench $(BENCH_DIR)/$(BENCH_NAME).`date +%Y-%m-%d.%H%M`

GETCONFDEPS = CONFDEP1=`ls _darcs/hashed_inventory 2>/dev/null` \
              CONFDEP2=`ls _darcs/patches/pending 2> /dev/null`
SETENV = env BUILD_RPATH=$(BUILD_RPATH) TESTHOOK="$(TESTHOOK)"

${FLAVOURS:%=$(OBJ)%/cmake.stamp}: Makefile CMakeLists.txt $(CONFDEP1) $(CONFDEP2) $(TOOLSTAMP)
	chmod +x test/divine # darcs does not remember +x on files
	mkdir -p $$(dirname $@)
	@if test -z "$(FLAVOUR)"; then echo "ERROR: FLAVOUR must be provided"; false; fi
	cd $$(dirname $@) && $(CMAKE) $(PWD) $($(FLAVOUR)_FLAGS) $(CMAKE_EXTRA) -G "$(GENERATOR)"
	touch $@

${TARGETS:%=static-%}:
	$(MAKE) $(OBJ)static/cmake.stamp $(GETCONFDEPS) FLAVOUR=static
	$(SETENV) $(CMAKE) --build $(OBJ)static --target ${@:static-%=%} -- $(EXTRA)

${TARGETS:%=debug-%}:
	$(MAKE) $(OBJ)debug/cmake.stamp $(GETCONFDEPS) FLAVOUR=debug
	$(SETENV) $(CMAKE) --build $(OBJ)debug --target ${@:debug-%=%} -- $(EXTRA)

${TARGETS:%=semidbg-%}:
	$(MAKE) $(OBJ)semidbg/cmake.stamp $(GETCONFDEPS) FLAVOUR=semidbg
	$(SETENV) $(CMAKE) --build $(OBJ)semidbg --target ${@:semidbg-%=%} -- $(EXTRA)

${TARGETS:%=release-%}:
	$(MAKE) $(OBJ)release/cmake.stamp $(GETCONFDEPS) FLAVOUR=release
	$(SETENV) $(CMAKE) --build $(OBJ)release --target ${@:release-%=%} -- $(EXTRA)

${TARGETS:%=bench-%}:
	$(MAKE) $(OBJ)bench/cmake.stamp $(GETCONFDEPS) FLAVOUR=bench
	$(SETENV) $(CMAKE) --build $(OBJ)bench --target ${@:bench-%=%} -- $(EXTRA)

${TARGETS:%=asan-%}:
	$(MAKE) $(OBJ)asan/cmake.stamp $(GETCONFDEPS) FLAVOUR=asan
	$(SETENV) $(CMAKE) --build $(OBJ)asan --target ${@:asan-%=%} -- $(EXTRA)

toolchain: $(TOOLSTAMP)

$(TOOLSTAMP):
	mkdir -p $(OBJ)toolchain
	cd $(OBJ)toolchain && $(CMAKE) $(PWD) $(toolchain_FLAGS) -G "$(GENERATOR)"
	$(CMAKE) --build $(OBJ)toolchain --target unwind_static -- $(EXTRA)
	$(CMAKE) --build $(OBJ)toolchain --target cxxabi_static -- $(EXTRA)
	$(CMAKE) --build $(OBJ)toolchain --target cxx_static -- $(EXTRA)
	$(CMAKE) --build $(OBJ)toolchain --target cxx -- $(EXTRA)
	$(CMAKE) --build $(OBJ)toolchain --target clang -- $(EXTRA)
	mkdir -p $(CXX_STATIC)
	ln -sf $(CXX_LDIR)/libc++{,abi}.a $(CXX_STATIC)/
	touch $@

CURSES = libncursesw.a libncurses.a libcurses.a

${FLAVOURS:%=%-env}:
	$(MAKE) ${@:%-env=%} ${@:%-env=%}-llvm-utils
	env PATH=$(OBJ)toolchain/clang/bin:$(OBJ)${@:%-env=%}/llvm/bin:$(OBJ)${@:%-env=%}/tools:$$PATH \
		CXXFLAGS="$(CXXFLAGS_)" LDFLAGS="$(LDFLAGS_)" \
		DIVINE_BUILD_ENV="${@:%-env=%}" $$SHELL

env : ${DEFAULT_FLAVOUR}-env

show: # make show var=VAR
	@echo $($(var))

.PHONY: ${TARGETS} ${FLAVOURS} ${TARGETS:%=release-%} ${FLAVOURS:%=%-env} toolchain validate dist env

dist:
	darcs dist -d divine-$$(cat releng/version).$$(cat releng/patchlevel)$(VERSION_APPEND)

validate:
	$(MAKE) semidbg-divine
	$(MAKE) semidbg-test-divine
	$(OBJ)semidbg/divine/test-divine
	$(MAKE) semidbg-functional TAGS=min

${FLAVOURS:%=%-ext}:
	$(MAKE) ${@:%-ext=%} ${@:%-ext=%}-runner
	cd $(OBJ)${@:%-ext=%}/test && \
	   $(SETENV) SRCDIR=$(PWD) bash $(PWD)/test/lib/testsuite --testdir $(PWD)/test --only ext-

toolchain-install: toolchain
	$(CMAKE) --build $(OBJ)toolchain --target install -- $(EXTRA)

install: toolchain-install

prerequisites:
	sh ./releng/install-prereq.sh
