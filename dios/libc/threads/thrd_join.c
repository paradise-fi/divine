// (c) 2018 Vladimír Štill

#include <threads.h>

int thrd_join( thrd_t thr, int *res ) {
    void *pres;
    int r = pthread_join( thr, &pres );
    if ( res )
        *res = (uintptr_t)pres;
    return r == 0 ? thrd_success : thrd_error;
}
