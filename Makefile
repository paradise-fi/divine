-include local.make
CC ?= cc
CXX ?= c++
GENERATOR ?= $(if $(shell ninja --version 2> /dev/null || ninja-build --version 2> /dev/null),Ninja,"Unix Makefiles")
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

$(OBJ)%/configure-stamp: CMakeLists.txt
	echo configuring $*
	mkdir -p $(OBJ)$*
	cd $(OBJ)$* && cmake $(PWD) $($*_FLAGS) -G $(GENERATOR)
	touch $@

$(OBJ)toolchain/build-stamp: $(OBJ)toolchain/configure-stamp
	cmake --build $(OBJ)toolchain --target cxx $(VERB)
	cmake --build $(OBJ)toolchain --target clang $(VERB)
	cmake --build $(OBJ)toolchain --target compiler-rt $(VERB)
	touch $@

toolchain: $(OBJ)toolchain/build-stamp

debug-%: toolchain $(OBJ)debug/configure-stamp
	cmake --build $(OBJ)debug --target $* $(VERB)
asan-%: toolchain $(OBJ)asan/configure-stamp
	cmake --build $(OBJ)asan --target $* $(VERB)
release-%: toolchain $(OBJ)release/configure-stamp
	cmake --build $(OBJ)release --target $* $(VERB)

debug-env : debug
	env PATH=$(OBJ)debug/clang/bin:$(OBJ)debug/llvm/bin:$(OBJ)debug/tools:$$PATH \
		CXXFLAGS="$(CXXFLAGS_)" LDFLAGS="$(LDFLAGS_)" $$SHELL

asan-env : asan
	env PATH=$(OBJ)asan/clang/bin:$(OBJ)asan/llvm/bin:$(OBJ)asan/tools:$$PATH \
		CXXFLAGS="$(CXXFLAGS_)" LDFLAGS="$(LDFLAGS_)" $$SHELL

release-env : release
	env PATH=$(OBJ)release/clang/bin:$(OBJ)release/llvm/bin:$(OBJ)release/tools:$$PATH \
		CXXFLAGS="$(CXXFLAGS_)" LDFLAGS="$(LDFLAGS_)" $$SHELL

env : debug-env

website: debug-website
debug: debug-divine debug-lart
asan: asan-divine asan-lart
release: release-divine release-lart
check: debug-check
unit: debug-unit
unit-%: debug-unit-%

# make being too smart here?
.PRECIOUS: $(OBJ)toolchain/configure-stamp $(OBJ)toolchain/build-stamp $(OBJ)debug/configure-stamp $(OBJ)release/configure-stamp $(OBJ)asan/configure-stamp
.PHONY: toolchain website debug debug-% release release-% check unit unit-% debug-env release-env env

dist:
	cmake --build $(OBJ)debug --target package_source $(VERB)
	cp $(OBJ)debug/divine-*.tar.gz .
