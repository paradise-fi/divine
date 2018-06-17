/* TAGS: sym c++ */
/* VERIFY_OPTS: --symbolic */
/* CC_OPTS: */

// V: v.O0 CC_OPT: -O0 TAGS: min
// V: v.O1 CC_OPT: -O1
// V: v.O2 CC_OPT: -O2
// V: v.Os CC_OPT: -Os
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
    int v = __sym_val_i32();
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
