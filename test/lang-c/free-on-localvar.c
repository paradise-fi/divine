/* TAGS: min c */
#include <stdlib.h>

int main()
{
    int a;
    free( &a ); /* ERROR */
}
