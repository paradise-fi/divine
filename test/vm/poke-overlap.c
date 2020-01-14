/* TAGS: c */
#include <sys/divm.h>
#include <sys/trace.h>
#include <assert.h>

char a[ 16 ];
__vm_pointer_t ptr;

void poke( int off, int len, int val )
{
    __vm_poke( _VM_ML_User, ptr.obj, ptr.off + off, len, val );
}

int main()
{
    ptr = __vm_pointer_split( &a );
    poke(  0, 3, 2 );
    poke( 10, 5, 3 );
    poke(  0, 1, 1 );

    __vm_meta_t meta = { 0, 0, 0 };
    int i = 0;

    do {
        int off = meta.offset + meta.length, len = 16 - off;
        meta = __vm_peek( _VM_ML_User, ptr.obj, off, len );

        __dios_trace_f( "%d %d %d", i, meta.offset, meta.length );

        switch ( i )
        {
            case 0: assert( meta.offset == 0 && meta.length == 1 ); break;
            case 1: assert( meta.offset == 1 && meta.length == 2 ); break;
            case 2: assert( meta.offset == 10 && meta.length == 5 ); break;
        }

        if ( meta.length )
            assert( meta.value == ++ i );
    }
    while ( meta.length );

    assert( i == 3 );

    return 0;
}
