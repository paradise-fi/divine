/* TAGS: c */
#include <stdio.h>
#include <assert.h>

int write()
{
    printf( "foo" );
    return 42;
}

int main()
{
    assert( write() == 42 );
}
