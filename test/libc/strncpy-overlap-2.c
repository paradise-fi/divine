#include <string.h>

int main()
{
    char buf[ 15 ] = "xyz";
    strncpy( buf, buf + 1, 2 ); /* ERROR */
}
