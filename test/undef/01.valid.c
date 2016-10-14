#include <dios.h>

void thread( void *x ) {}
int main() {
    int tid = __dios_start_thread( thread, 0, 0 );
    if ( tid )
        return 0;
    return 1;
}
