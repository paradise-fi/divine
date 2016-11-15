#include <assert.h>
#include <divine.h>

int main() {
    char *array = __vm_obj_make( 12 );
    __vm_obj_resize( array + 4, 20 ); /* ERROR */
}
