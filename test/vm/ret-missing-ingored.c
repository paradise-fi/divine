/* TAGS: min c */
/* VERIFY_OPTS: -o ignore:control */

#include <assert.h>

typedef int (*IntFun)();

void voidFun() { }

int main() {
    int x = ((IntFun)voidFun)();
    assert( x == 0 );
}
