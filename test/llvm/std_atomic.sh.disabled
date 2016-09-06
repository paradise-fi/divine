. lib
. flavour vanilla

test "$GEN_LLVM" = "ON" || skip

llvm_precompile

compile_atomic() {
    ann divine compile --llvm --cflags="-std=c++11" data/llvm/$1 --precompiled=. >& progress
}

compile_atomic atomic_interface.cpp
llvm_verify_file atomic_interface.bc valid

compile_atomic atomic.cpp
llvm_verify_file atomic.bc valid
