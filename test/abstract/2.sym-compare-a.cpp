/* VERIFY_OPTS: --symbolic */

#include <cassert>
#define __sym __attribute__((__annotate__("lart.abstract.sym")))

int main() {
    __sym int x;
    int y = 0;
    assert( x != y ); /* ERROR */
}
