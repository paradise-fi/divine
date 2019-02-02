/* TAGS: min c */
#include <assert.h>
#include <stdlib.h>
#include <sys/divm.h>

struct Obj
{
    struct Obj* ptr;
};

int main()
{
    struct Obj **arr = __vm_obj_make( 3 * sizeof( struct Obj* ), _VM_PT_Heap );

    for( int i = 0; i < 3; ++i )
        arr[i] = __vm_obj_make( sizeof( struct Obj ), _VM_PT_Heap );

    arr[1]->ptr = arr[1];

    struct Obj **new_arr = __vm_obj_clone( arr, NULL );
    assert( arr != new_arr );

    arr[2]->ptr = arr[2];
    assert( arr[2]->ptr );
    assert( arr[2]->ptr == arr[2] );

    for( int i = 0; i < 3; ++i )
        assert( arr[i] != new_arr[i] );

    assert( arr[1]->ptr == arr[1] );
    assert( new_arr[1]->ptr == new_arr[1] );
    assert( arr[1]->ptr != new_arr[1]->ptr );

    assert( new_arr[2]->ptr ); /* ERROR: uninitialised */
}
