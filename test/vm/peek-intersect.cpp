#include <sys/divm.h>
#include <sys/trace.h>
#include <cassert>

int main()
{
    int *x = (int *) __vm_obj_make( 8, _VM_PT_Heap );
    __vm_pointer_t ptr = __vm_pointer_split( x );
    __vm_poke( _VM_ML_User, ptr.obj, 4, 4, 42 );
    __vm_meta_t m = __vm_peek( _VM_ML_User, ptr.obj, 0, 6 );
    __dios_trace_f( "m: %d %d", m.offset, m.length );
    assert( m.offset == 4 );
    assert( m.length == 4 );
    __vm_obj_free( x );
}
