#include <string.h>

int main()
{
    char buf[ 15 ] = "bx";
    strcpy( buf, buf + 1 ); /* ERROR */
}
