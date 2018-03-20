// (c) 2018 Vladimír Štill

#include <threads.h>

_PDCLIB_noreturn void thrd_exit( int res ) {
    pthread_exit( (void *)((uintptr_t) res) );
}
