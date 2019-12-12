#include <string.h>
#include <assert.h>

int main()
{
    char buf[ 15 ] = "b";
    strncpy( buf, buf + 2, 2 );
    assert( !strcmp( buf, buf + 2 ) );
}
