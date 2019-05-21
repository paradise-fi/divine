/* TAGS: min c */
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>

int main() {
    int foo[4];
    foo[0] = 0;

    uintptr_t a = (uintptr_t) foo;
    uintptr_t lowbits = a & 0xffffffff;
    a >>= 32;
    uintptr_t b = a;
    b <<= 32;
    b |= lowbits;
    int *bar = (void *) b;
    bar[0] ++;
    assert( foo[0] == 1 );

    uint32_t c = a;
    uintptr_t d = c;
    d <<= 32;
    d |= lowbits;

    int *baz = (int *) d;
    baz[0] ++;

    assert( foo[0] == 2 );
}
