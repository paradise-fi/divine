GENERATOR != if ninja --version > /dev/null 2>&1 || \
                ninja-build --version > /dev/null 2>&1; then echo Ninja; else echo Unix Makefiles; fi
# fallback
GENERATOR ?= Unix Makefiles

-include /etc/divine.make # for system-wide config
-include local.make

CC ?= cc
CXX ?= c++
DEFAULT_FLAVOUR ?= release

MAKEFLAGS ?= --no-print-directory
CONFIG ?= -DBUILD_SHARED_LIBS=ON
OBJ ?= $(PWD)/_build.
EXTRA != if test "$(GENERATOR)" = Ninja && test -n "$(VERBOSE)"; then echo -v -d explain; fi; \
         if test -n "$(JOBS)"; then echo -j $(JOBS); fi

TOOLDIR = $(OBJ)toolchain/
CLANG = $(TOOLDIR)/clang/
RTBIN = $(TOOLDIR)/runtime
RTSRC = $(PWD)/runtime

LDFLAGS_ = -L$(RTBIN)/libunwind/src -Wl,-rpath,$(RTBIN)/libunwind/src \
           -L$(RTBIN)/libcxxabi/src -Wl,-rpath,$(RTBIN)/libcxxabi/src \
           -L$(RTBIN)/libcxx/lib -Wl,-rpath,$(RTBIN)/libcxx/lib \

CXXFLAGS_ = -isystem $(RTSRC)/libcxxabi/include -isystem $(RTSRC)/libcxx/include \
            -isystem $(RTSRC)/libunwind/include \
            -stdlib=libc++ -nostdinc++ -Wno-unused-command-line-argument

TOOLCHAIN = -DCMAKE_C_COMPILER=$(CLANG)/bin/clang \
	    -DCMAKE_CXX_COMPILER=$(CLANG)/bin/clang++ \
	    -DCMAKE_CXX_FLAGS="$(CXXFLAGS_)" \
	    -DCMAKE_EXE_LINKER_FLAGS="$(LDFLAGS_)" -DCMAKE_SHARED_LINKER_FLAGS="$(LDFLAGS_)"

release_FLAGS = -DCMAKE_BUILD_TYPE=RelWithDebInfo $(TOOLCHAIN) $(CONFIG)
debug_FLAGS = -DCMAKE_BUILD_TYPE=Debug $(TOOLCHAIN) $(CONFIG)
asan_CXXFLAGS = -fsanitize=address -fno-omit-frame-pointer -fno-optimize-sibling-calls -g -O1
asan_FLAGS = $(debug_FLAGS) -DCMAKE_CXX_FLAGS_DEBUG="$(asan_CXXFLAGS)"

toolchain_FLAGS = -DCMAKE_BUILD_TYPE=RelWithDebInfo -DTOOLCHAIN=ON \
		  -DCMAKE_CXX_COMPILER=$(CXX) -DCMAKE_C_COMPILER=$(CC) \
		  -DBUILD_SHARED_LIBS=ON

all: $(DEFAULT_FLAVOUR)

FLAVOURS = debug asan release
TARGETS = divine unit functional website check llvm-dis clang test-divine

${TARGETS}:
	$(MAKE) $(DEFAULT_FLAVOUR)-$@

${FLAVOURS}:
	$(MAKE) $@-divine

GETCONFDEPS = CONFDEP1=`ls _darcs/hashed_inventory 2>/dev/null` \
              CONFDEP2=`ls _darcs/patches/pending 2> /dev/null`

${FLAVOURS:%=$(OBJ)%/cmake.stamp}: Makefile CMakeLists.txt $(CONFDEP1) $(CONFDEP2) $(OBJ)toolchain/stamp
	chmod +x test/divine # darcs does not remember +x on files
	mkdir -p $$(dirname $@)
	@if test -z "$(FLAVOUR)"; then echo "ERROR: FLAVOUR must be provided"; false; fi
	cd $$(dirname $@) && cmake $(PWD) $($(FLAVOUR)_FLAGS) -G "$(GENERATOR)"
	touch $@

${TARGETS:%=debug-%}:
	$(MAKE) $(OBJ)debug/cmake.stamp $(GETCONFDEPS) FLAVOUR=debug
	cmake --build $(OBJ)debug --target ${@:debug-%=%} -- $(EXTRA)

${TARGETS:%=release-%}:
	$(MAKE) $(OBJ)release/cmake.stamp $(GETCONFDEPS) FLAVOUR=release
	cmake --build $(OBJ)release --target ${@:release-%=%} -- $(EXTRA)

${TARGETS:%=asan-%}:
	$(MAKE) $(OBJ)asan/cmake.stamp $(GETCONFDEPS) FLAVOUR=asan
	cmake --build $(OBJ)asan --target ${@:asan-%=%} -- $(EXTRA)

toolchain: $(OBJ)toolchain/stamp

$(OBJ)toolchain/stamp:
	mkdir -p $(OBJ)toolchain
	cd $(OBJ)toolchain && cmake $(PWD) $(toolchain_FLAGS) -G "$(GENERATOR)"
	cmake --build $(OBJ)toolchain --target cxx -- $(EXTRA)
	cmake --build $(OBJ)toolchain --target clang -- $(EXTRA)
	cmake --build $(OBJ)toolchain --target compiler-rt -- $(EXTRA)
	cmake --build $(OBJ)toolchain --target llvm-dis -- $(EXTRA)
	cmake --build $(OBJ)toolchain --target llvm-as -- $(EXTRA)
	cmake --build $(OBJ)toolchain --target llc  -- $(EXTRA)
	touch $@

${FLAVOURS:%=%-env}:
	$(MAKE) ${@:%-env=%}
	env PATH=$(OBJ)toolchain/clang/bin:$(OBJ)toolchain/llvm/bin:$(OBJ)${@:%-env=%}/tools:$$PATH \
		CXXFLAGS="$(CXXFLAGS_)" LDFLAGS="$(LDFLAGS_)" $$SHELL

env : debug-env

show: # make show var=VAR
	@echo $($(var))

.PHONY: ${TARGETS} ${FLAVOURS} ${TARGETS:%=release-%} ${FLAVOURS:%=%-env} toolchain validate dist

dist:
	$(MAKE) $(OBJ)debug/cmake.stamp $(GETCONFDEPS)
	cmake --build $(OBJ)debug --target package_source $(EXTRA)
	cp $(OBJ)debug/divine-*.tar.gz .

validate:
	$(MAKE) debug-divine
	$(MAKE) release-divine
	$(MAKE) debug-test-divine
	$(OBJ)/debug/divine/test-divine
	$(MAKE) debug-functional T=[.]1[.][^.]+$
	$(MAKE) release-functional T=[.][12][.][^.]+$
