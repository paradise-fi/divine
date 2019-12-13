#include <sys/divm.h>
#include <stdlib.h>

__attribute__((__nothrow__))
void free( void * p )
{
    if ( !p )
        return;

    __vm_obj_free( p );
}
