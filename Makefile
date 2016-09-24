GENERATOR != if ninja --version > /dev/null 2>&1 || \
                ninja-build --version > /dev/null 2>&1; then echo Ninja; else echo Unix Makefiles; fi

-include local.make
CC ?= cc
CXX ?= c++

MAKEFLAGS ?= --no-print-directory
CONFIG ?= -DBUILD_SHARED_LIBS=ON
OBJ ?= $(PWD)/_build.
VERB = $(if $(filter $(GENERATOR),Ninja),-- $(if $(VERBOSE),-v))

TOOLDIR = $(OBJ)toolchain/
CLANG = $(TOOLDIR)/clang/
RTBIN = $(TOOLDIR)/runtime
RTSRC = $(PWD)/runtime

LDFLAGS_ = -L$(RTBIN)/libunwind/src -Wl,-rpath,$(RTBIN)/libunwind/src \
           -L$(RTBIN)/libcxxabi/src -Wl,-rpath,$(RTBIN)/libcxxabi/src \
           -L$(RTBIN)/libcxx/lib -Wl,-rpath,$(RTBIN)/libcxx/lib \

CXXFLAGS_ = -isystem $(RTSRC)/libcxxabi/include -isystem $(RTSRC)/libcxx/include \
            -isystem $(RTSRC)/libunwind/include \
            -stdlib=libc++ -nostdinc++

TOOLCHAIN = -DCMAKE_C_COMPILER=$(CLANG)/bin/clang \
	    -DCMAKE_CXX_COMPILER=$(CLANG)/bin/clang++ \
	    -DCMAKE_CXX_FLAGS="$(CXXFLAGS_)" \
	    -DCMAKE_EXE_LINKER_FLAGS="$(LDFLAGS_)" -DCMAKE_SHARED_LINKER_FLAGS="$(LDFLAGS_)"

release_FLAGS = -DCMAKE_BUILD_TYPE=RelWithDebInfo $(TOOLCHAIN) $(CONFIG)
debug_FLAGS = -DCMAKE_BUILD_TYPE=Debug $(TOOLCHAIN) $(CONFIG)
asan_FLAGS = $(debug_FLAGS) -DCMAKE_CXX_FLAGS_DEBUG="-fsanitize=address -fno-omit-frame-pointer -fno-optimize-sibling-calls -g -O1"
toolchain_FLAGS = -DCMAKE_BUILD_TYPE=RelWithDebInfo -DTOOLCHAIN=ON \
		  -DCMAKE_CXX_COMPILER=$(CXX) -DCMAKE_C_COMPILER=$(CC) \
		  -DBUILD_SHARED_LIBS=ON

all: debug

FLAVORS = debug asan release
TARGETS = divine unit functional website check llvm-dis clang

${TARGETS}:
	$(MAKE) debug-$@

${FLAVORS}:
	$(MAKE) $@-divine

${FLAVORS:%=.stamp-%-configure}: CMakeLists.txt .stamp-toolchain
	@echo configuring $@
	mkdir -p $(OBJ)${@:.stamp-%-configure=%}
	cd $(OBJ)${@:.stamp-%-configure=%} && \
	    cmake $(PWD) $($(@:.stamp-%-configure=%)_FLAGS) -G "$(GENERATOR)"
	touch $@

${FLAVORS:%=.stamp-%-build}:
	$(MAKE) ${@:%-build=%-configure}
	cmake --build $(OBJ)${@:.stamp-%-build=%} --target ${@:.stamp-%-build=%} $(VERB)

${TARGETS:%=debug-%}: .stamp-debug-configure
	cmake --build $(OBJ)debug --target ${@:debug-%=%} $(VERB)

${TARGETS:%=release-%}: .stamp-release-configure
	cmake --build $(OBJ)release --target ${@:release-%=%} $(VERB)

${TARGETS:%=asan-%}: .stamp-asan-configure
	cmake --build $(OBJ)asan --target ${@:asan-%=%} $(VERB)

.stamp-toolchain:
	mkdir -p $(OBJ)toolchain
	cd $(OBJ)toolchain && cmake $(PWD) $(toolchain_FLAGS) -G "$(GENERATOR)"
	cmake --build $(OBJ)toolchain --target cxx $(VERB)
	cmake --build $(OBJ)toolchain --target clang $(VERB)
	cmake --build $(OBJ)toolchain --target compiler-rt $(VERB)
	touch $@

${FLAVORS:%=%-env}:
	$(MAKE) ${@:%-env=%}
	env PATH=$(OBJ)${@:%-env=%}/clang/bin:$(OBJ)${@:%-env=%}/llvm/bin:$(OBJ)${@:%-env=%}/tools:$$PATH \
		CXXFLAGS="$(CXXFLAGS_)" LDFLAGS="$(LDFLAGS_)" $$SHELL

env : debug-env

show: # make show var=VAR
	@echo $($(var))

# make being too smart here?
.PHONY: ${TARGETS} ${FLAVORS} ${TARGETS:%=release-%} ${FLAVORS:%=%-env}

dist:
	cmake --build $(OBJ)debug --target package_source $(VERB)
	cp $(OBJ)debug/divine-*.tar.gz .
