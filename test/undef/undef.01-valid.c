/* TAGS: min c */
#include <dios.h>

void thread( void *x ) { __dios_suicide(); }
int main() {
    void *tid = __dios_start_task( thread, 0, 0 );
    if ( tid )
        return 0;
    return 1;
}
