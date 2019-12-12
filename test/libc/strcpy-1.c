#include <string.h>
#include <assert.h>

int main()
{
    char buf[ 15 ] = "b";
    strcpy( buf + 2, buf );
    assert( !strcmp( buf, buf + 2 ) );
}
