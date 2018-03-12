/* TAGS: sym min c++ */
/* VERIFY_OPTS: --symbolic */
#include <rst/domains.h>
#include <cassert>

void init( int * i, int v ) {
    *i = v % 10;
}

int main() {
    int i;
    _SYM int v;
    init( &i, v );
    assert( i < 10 );
}
