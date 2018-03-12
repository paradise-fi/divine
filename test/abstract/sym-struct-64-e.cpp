/* TAGS: sym min c++ */
/* VERIFY_OPTS: --symbolic */
#include <rst/domains.h>
#include <cstdint>
#include <cassert>

struct Widget {
    int64_t i;
};

void init( Widget * w ) {
    _SYM int v;
    w->i = v % 10;
}

bool check( Widget * w ) {
    return w->i < 10;
}

int main() {
    Widget w;
    init( &w );
    assert( check( &w ) );
}

