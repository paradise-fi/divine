/* VERIFY_OPTS: --symbolic */

#include <cassert>
#define __sym __attribute__((__annotate__("lart.abstract.symbolic")))

int main() {
    __sym int array[ 4 ];
    if ( array[ 4 ] ) /* ERROR */
        return 0;
    return 1;
}
