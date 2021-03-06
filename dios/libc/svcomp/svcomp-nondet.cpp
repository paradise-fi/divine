#include <stdint.h>
#include <assert.h>
#include <stdbool.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wuninitialized"

#define _SVC_NONDET(t,n,bw) t __VERIFIER_nondet_ ## n() { return __lamp_any_i ## bw(); }

extern "C" {

    uint8_t  __lamp_any_i1();
    uint8_t  __lamp_any_i8();
    uint16_t __lamp_any_i16();
    uint32_t __lamp_any_i32();
    uint64_t __lamp_any_i64();

    _SVC_NONDET(bool, bool, 8)
    _SVC_NONDET(char, char, 8)
    _SVC_NONDET(unsigned char, uchar, 8)
    _SVC_NONDET(int, int, 32)
    _SVC_NONDET(short, short, 16)
    _SVC_NONDET(long, long, 64)
    _SVC_NONDET(unsigned short, ushort, 16)
    _SVC_NONDET(unsigned int, uint, 32)
    _SVC_NONDET(unsigned long, ulong, 64)
    _SVC_NONDET(unsigned, unsigned, 32)
    _SVC_NONDET(uint64_t, pointer, 64)

}

#pragma clang diagnostic pop
