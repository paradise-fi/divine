#include <sys/divm.h>
#include <sys/fault.h>
#include <sys/vmutil.h>
#include <stdlib.h>

__attribute__((__nothrow__))
void free( void * p )
{
    if ( !p )
        return;

    if ( __dios_pointer_get_type( p ) != _VM_PT_Heap )
        __dios_fault( _VM_F_Memory, "invalid free of a non-heap pointer" );

    __vm_obj_free( p );
}
