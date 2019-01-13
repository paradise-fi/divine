/* TAGS: c */
#include <assert.h>

int write()
{
    return 42;
}

int main()
{
    assert( write() == 42 );
}
