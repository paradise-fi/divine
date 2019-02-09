#include <stdio.h>
#include <stdlib.h>

void __dios_fault( int f, const char *msg )
{
    (void) f;
    fprintf( stderr, "%s", msg );
    abort();
}
