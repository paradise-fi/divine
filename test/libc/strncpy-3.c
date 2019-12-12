#include <string.h>

int main()
{
    char buf[ 15 ] = "";
    strncpy( buf + 1, buf, 1 );
}
