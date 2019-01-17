/* TAGS: c */
#include <string.h>
#include <assert.h>

int chmod( const char * a, int b )
{
    return 2;
}

int getpid()
{
    return 7;
}

const char * lseek()
{
    return "foo";
}

int main()
{
    assert( chmod( "file", 0666 ) == 2 );
    assert( getpid() == 7 );
    assert( strcmp( lseek(), "foo" ) == 0 );

    return 0;
}
