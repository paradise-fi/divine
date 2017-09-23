/* VERIFY_OPTS: --symbolic */
#include <abstract/domains.h>
#include <cassert>

int main() {
    int array[ 4 ] = { 0 };
    _SYM int x;
    assert( x == array[ 0 ] ); /* ERROR */
}
