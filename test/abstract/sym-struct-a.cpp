/* TAGS: sym todo min c++ */
/* VERIFY_OPTS: --symbolic */
#include <rst/domains.h>
#include <cassert>

struct S {
    int x;
};

int main() {
    _SYM int x;
    S s;
    s.x = x;
    assert( s.x != 0 ); /* ERROR */
}
