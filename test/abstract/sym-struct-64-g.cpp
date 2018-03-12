/* TAGS: sym min c++ */
/* VERIFY_OPTS: --symbolic */
#include <rst/domains.h>
#include <cstdint>
#include <cassert>


struct Widget{
    int64_t i;
};

struct Store{
    Widget * w;
};

void init( Widget * w ) {
    _SYM int v;
    w->i = v % 10;
}

void init( Store * s, Widget * w ) {
    s->w = w;
}

bool check( Store * s ) {
    return s->w->i < 9;
}

int main() {
    Store store;
    Widget w;
    init( &w );
    init( &store, &w  );
    // value in store could be 9!
    assert( check( &store ) ); /* ERROR */
};
