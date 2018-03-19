// (c) 2018 Vladimír Štill

#include <threads.h>

int thrd_detach( thrd_t thr ) {
    if ( pthread_detach( thr ) == 0 )
        return thrd_success;
    return thrd_error;
}
