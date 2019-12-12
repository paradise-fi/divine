#include <string.h>
#include <assert.h>
#include <sys/trace.h>

int main()
{
    char buf[ 15 ] = "b\0x";
    strncpy( buf + 1, buf, 1 );
    assert( buf[ 2 ] == 'x' );
    buf[ 2 ] = 0;
    __dios_trace_f( "buf = '%s'", buf );
    assert( !strcmp( buf, "bb" ) );
}
