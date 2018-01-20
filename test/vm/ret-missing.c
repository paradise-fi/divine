/* TAGS: min c */
#include <assert.h>

typedef int (*IntFun)();

void voidFun() { }

int main() {
    int x = ((IntFun)voidFun)(); /* ERROR */
    assert( x == 0 );
}
