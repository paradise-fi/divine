#include <assert.h>

int __VERIFIER_nondet_int();

int main() {
    int array[2];
    int x = __VERIFIER_nondet_int();
    array[1] = 42;
    if ( x == 1 )
        assert( array[x] == 42 );
    return 0;
}
