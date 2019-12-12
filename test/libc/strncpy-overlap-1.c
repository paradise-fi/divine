#include <string.h>

int main()
{
    char buf[ 15 ] = "xyz";
    strncpy( buf + 1, buf, 2 ); /* ERROR */
}
