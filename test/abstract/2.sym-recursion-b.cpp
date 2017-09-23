/* VERIFY_OPTS: --symbolic */
#include <abstract/domains.h>
#include <cassert>
#include <limits>
#include <sys/vmutil.h>

int zero( int a ) {
    if ( a % 2 == 0 )
        return 42;
    else
        return zero( a - 1 );
}

int main() {
    _SYM int a;
    assert( zero( a ) == 42 );
    return 0;
}
