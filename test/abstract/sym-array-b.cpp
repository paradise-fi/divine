/* TAGS: sym todo min c++ */
/* VERIFY_OPTS: --symbolic */
#include <rst/domains.h>
#include <cassert>

int main() {
    _SYM int array[ 4 ];
    if ( array[ 4 ] ) /* ERROR */
        return 0;
    return 1;
}
