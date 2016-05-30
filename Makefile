-include local.make
CC ?= cc
CXX ?= c++
GENERATOR ?= Ninja
CONFIG ?= -DBUILD_SHARED_LIBS=ON
OBJ ?= $(PWD)/_build.
VERB = $(if $(filter $(GENERATOR),Ninja),-- $(if $(VERBOSE),-v))

TOOLDIR = $(OBJ)toolchain/
CLANG = $(TOOLDIR)/clang/
RTBIN = $(TOOLDIR)/runtime
RTSRC = $(PWD)/runtime

CXXFLAGS_ = -L$(RTBIN)/libunwind/src -Wl,-rpath,$(RTBIN)/libunwind/src \
            -L$(RTBIN)/libcxxabi/src -Wl,-rpath,$(RTBIN)/libcxxabi/src \
            -L$(RTBIN)/libcxx/lib -Wl,-rpath,$(RTBIN)/libcxx/lib \
            -isystem $(RTSRC)/libcxxabi/include -isystem $(RTSRC)/libcxx/include \
            -isystem $(RTSRC)/libunwind/include \
            -stdlib=libc++

TOOLCHAIN = -DCMAKE_C_COMPILER=$(CLANG)/bin/clang \
	    -DCMAKE_CXX_COMPILER=$(CLANG)/bin/clang++ \
	    -DCMAKE_CXX_FLAGS="$(CXXFLAGS_)"

release_FLAGS = -DCMAKE_BUILD_TYPE=RelWithDebInfo $(TOOLCHAIN) $(CONFIG)
debug_FLAGS = -DCMAKE_BUILD_TYPE=Debug $(TOOLCHAIN) $(CONFIG)
toolchain_FLAGS = -DCMAKE_BUILD_TYPE=RelWithDebInfo -DTOOLCHAIN=ON \
		  -DCMAKE_CXX_COMPILER=$(CXX) -DCMAKE_C_COMPILER=$(CC) \
		  -DBUILD_SHARED_LIBS=ON

all: debug

$(OBJ)%/configure-stamp: CMakeLists.txt
	echo configuring $*
	mkdir -p $(OBJ)$*
	cd $(OBJ)$* && cmake $(PWD) $($*_FLAGS) -G $(GENERATOR)
	touch $@

$(OBJ)toolchain/build-stamp: $(OBJ)toolchain/configure-stamp
	cmake --build $(OBJ)toolchain --target cxx $(VERB)
	cmake --build $(OBJ)toolchain --target clang $(VERB)
	touch $@

toolchain: $(OBJ)toolchain/build-stamp

debug-%: toolchain $(OBJ)debug/configure-stamp
	cmake --build $(OBJ)debug --target $* $(VERB)
release-%: toolchain $(OBJ)release/configure-stamp
	cmake --build $(OBJ)release --target $* $(VERB)

debug: debug-divine debug-lart
release: release-divine release-lart
check: debug-check
unit: debug-unit
unit-%: debug-unit-%

# make being too smart here?
.PRECIOUS: $(OBJ)toolchain/configure-stamp $(OBJ)toolchain/build-stamp $(OBJ)debug/configure-stamp $(OBJ)release/configure-stamp
.PHONY: toolchain

dist:
	cmake --build $(OBJ)debug --target package_source $(VERB)
	cp $(OBJ)debug/divine-*.tar.gz .
