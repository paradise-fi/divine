/* TAGS: todo sym */
/* VERIFY_OPTS: --symbolic */
#include <assert.h>

int __VERIFIER_nondet_int();

int main() {
    int array[2];
    int x = __VERIFIER_nondet_int();
    if ( x == 1 )
        assert( 42 / x == 42 );
    return 0;
}
