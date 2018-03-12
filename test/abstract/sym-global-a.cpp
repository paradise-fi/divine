/* TAGS: sym min c++ */
/* VERIFY_OPTS: --symbolic */
#include <rst/domains.h>
#include <cassert>

int x;

int get() { return x; }

int main() {
    _SYM int val;
    x = val;

    int y = get();
    assert( x == y );
}
