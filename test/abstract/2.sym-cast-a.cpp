/* VERIFY_OPTS: --symbolic */
#include <abstract/domains.h>
#include <cassert>

int main() {
    _SYM int x;
    if ( x < 0 )
        return 0;
    short y = x;
    ++y;
    assert( x + 1 == y ); /* ERROR */
}
