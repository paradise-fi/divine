/* VERIFY_OPTS: --symbolic */

#include <cassert>
#define __sym __attribute__((__annotate__("lart.abstract.sym")))

struct S {
    int x;
};

int main() {
    __sym int x;
    S s;
    s.x = x;
    assert( s.x != 0 ); /* ERROR */
}
