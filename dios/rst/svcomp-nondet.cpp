#include <rst/domains.h>
#include <stdint.h>
#include <assert.h>
#include <stdbool.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wuninitialized"

#define _SVC_NONDET(t,n) t __VERIFIER_nondet_ ## n () noexcept { _SYM t x; return x; }

extern "C" {

    _SVC_NONDET(bool, bool)
    _SVC_NONDET(char, char)
    _SVC_NONDET(int, int)
    _SVC_NONDET(short, short)
    _SVC_NONDET(long, long)
    _SVC_NONDET(unsigned int, uint)
    _SVC_NONDET(unsigned long, ulong)
    _SVC_NONDET(unsigned, unsigned)

}

#pragma clang diagnostic pop
