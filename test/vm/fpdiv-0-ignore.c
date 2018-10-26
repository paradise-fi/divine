/* TAGS: c */
/* VERIFY_OPTS: -o ignore:float */

#include <assert.h>

int main() {
    double zero = 0.0;
    double a = 3.14 / zero;
    assert( a != 0 );
}

