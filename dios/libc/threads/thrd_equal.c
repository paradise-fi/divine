// (c) 2018 Vladimír Štill

#include <threads.h>

int thrd_equal( thrd_t thr0, thrd_t thr1 ) {
    return pthread_equal( thr0, thr1 );
}
