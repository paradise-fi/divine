/*
 * Global
 * ======
 *
 *  Demonstration of a non-atomicity of integer incrementation.
 *
 *  *tags*: test, C99
 *
 * Description
 * -----------
 *
 *  This program is a simple demonstration of a non-atomicity of integer incrementation,
 *  showing that such operation should be protected with mutex to be immune from
 *  concurrent access.
 *
 *  When compiled with macro `BUG` defined, locking is disabled and therefore assertion
 *  violation is detected.
 *
 * Parameters
 * ----------
 *
 *  - `BUG`: if defined than the program is incorrect and violates the safety property
 *
 * Verification
 * ------------
 *
 *  - all available properties with the default values of parameters:
 *
 *         $ divine compile --llvm global.c
 *         $ divine verify -p assert global.bc -d
 *         $ divine verify -p safety global.bc -d
 *
 *  - introducing a bug:
 *
 *         $ divine compile --llvm --cflags="-DBUG" global.c -o global-bug.bc
 *         $ divine verify -p assert global-bug.bc -d
 *
 * Execution
 * ---------
 *
 *       $ clang -lpthread -o global.exe global.c
 *       $ ./global.exe
 */

#include <pthread.h>
#include <assert.h>
#include <stdlib.h>

volatile int i = 33;

#ifndef BUG
pthread_mutex_t mutex;
#endif

void* thread( void *x ) {
    (void)x;

#ifndef BUG
    pthread_mutex_lock( &mutex );
#endif
    i ++;
#ifndef BUG
    pthread_mutex_unlock( &mutex );
#endif

    return NULL;
}

int main() {
    pthread_t tid;

#ifndef BUG
    pthread_mutex_init( &mutex, NULL );
#endif
    pthread_create( &tid, NULL, thread, NULL );

#ifndef BUG
    pthread_mutex_lock( &mutex );
#endif
    i ++;
#ifndef BUG
    pthread_mutex_unlock( &mutex );
#endif

    pthread_join( tid, NULL );
    assert( i == 35 );
    return i;
}
