/* TAGS: sym min c++ */
/* VERIFY_OPTS: --symbolic */
#include <rst/domains.h>
#include <cassert>
#include <limits>

int main() {
    _SYM int x;
    if ( x < 0 )
        return 0;
    short y = x;
    ++y;
    assert( y != -32768 ); /* ERROR */
}
