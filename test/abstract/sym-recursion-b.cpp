/* TAGS: sym min c++ */
/* VERIFY_OPTS: --symbolic */
#include <rst/domains.h>
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
    int a = __sym_val_i32();
    assert( zero( a ) == 42 );
    return 0;
}
