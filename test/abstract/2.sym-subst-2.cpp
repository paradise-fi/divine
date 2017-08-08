/* VERIFY_OPTS: --symbolic */

#include <assert.h>

#define __sym __attribute__((__annotate__("lart.abstract.sym")))

int main() {
    __sym int x;
    long y = x;
    assert( y + 1 > y );
}
