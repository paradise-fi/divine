/* VERIFY_OPTS: --symbolic */

#include <cassert>
#define __sym __attribute__((__annotate__("lart.abstract.sym")))


int main() {
    int array[ 4 ] = { 0 };
    __sym int x;
    assert( x == array[ 0 ] ); /* ERROR */
}
