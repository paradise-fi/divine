/* TAGS: min c */
#include <assert.h>

int *test()
{
    int x = 3;
    return &x;
}

int main()
{
    int *x = test();
    assert( *x == 3 ); /* ERROR: invalid pointer dereference */
    return 0;
}
