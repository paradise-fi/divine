/* TAGS: min c */
#include <assert.h>
#include <sys/divm.h>

int main() {
    char *array = __vm_obj_make( 12 );
    __vm_obj_resize( array + 4, 20 ); /* ERROR */
}
