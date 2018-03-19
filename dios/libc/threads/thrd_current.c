// (c) 2018 Vladimír Štill

#include <threads.h>

thrd_t thrd_current( void ) {
    return pthread_self();
}
