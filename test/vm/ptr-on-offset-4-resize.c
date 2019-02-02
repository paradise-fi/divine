/* TAGS: min c */
#include <assert.h>
#include <sys/divm.h>

int main() {
    char *array = __vm_obj_make( 12, _VM_PT_Heap );
    int x;

    int **ptr = (int**)&array[4];
    *ptr = &x;
    **ptr = 42;
    assert( x == 42 );

    __vm_obj_resize( array, 20 );

    int y;
    int **ptr2 = (int**)&array[12];
    *ptr2 = 0;
    assert( *ptr == &x );
    assert( **ptr == 42 );

    *ptr2 = &y;
    **ptr2 = 2;
    assert( y == 2 );
    assert( x == 42 );

    assert( *ptr == &x );
    assert( *ptr2 == &y );
    assert( **ptr2 == 2 );
    assert( **ptr == 42 );

    *ptr = 0;

    assert( *ptr2 == &y );
    assert( **ptr2 == 2 );

    *ptr = &x;

    assert( *ptr == &x );
    assert( *ptr2 == &y );
    assert( **ptr2 == 2 );
    assert( **ptr == 42 );

    *ptr2 = 0;

    assert( *ptr == &x );
    assert( **ptr == 42 );

    __vm_obj_free( array );
}
