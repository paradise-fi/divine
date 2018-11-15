/* TAGS: min c */
/* VERIFY_OPTS: --leakcheck return */

#include <stdlib.h>

void foo() {
    int *x = malloc( sizeof( int ) );
}

int main() {
    foo(); /* ERROR */
}
