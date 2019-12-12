#include <string.h>

int main()
{
    char buf[ 15 ] = "b\0x";
    strcpy( buf + 1, buf ); /* ERROR */
}
