/* TAGS: min c */
#include <assert.h>

typedef int (*IntFun)();

short shortFun() { return 0; }

int main() {
    int x = ((IntFun)shortFun)(); /* ERROR */
    assert( x == 0 );
}
