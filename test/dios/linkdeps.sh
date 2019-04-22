# TAGS: min
. lib/testcase

divine check --dump-bc dump.bc $TESTS/lang-c/trivial.c
llvm-nm dump.bc
llvm-nm dump.bc | not grep '_Znwm$'
llvm-nm dump.bc | not grep __cxa_demangle
