/* TAGS: min c */
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>

int main() {
    int foo[4];
    foo[0] = 0;

    uintptr_t a = (uintptr_t) foo;
    a >>= 32;
    uintptr_t b = a;
    b <<= 32;
    int *bar = (void *) b;
    bar[0] ++;
    assert( foo[0] == 1 );

    uint32_t c = a;
    uintptr_t d = c;
    d <<= 32;

    int *baz = (int *) d;
    baz[0] ++;

    assert( foo[0] == 2 );
}
