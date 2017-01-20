#include <assert.h>

typedef int (*IntFun)();

long longFun() { return 0; }

int main() {
    int x = ((IntFun)longFun)(); /* ERROR */
    assert( x == 0 );
}
