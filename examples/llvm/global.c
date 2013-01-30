/*
 * This program is a simple demonstration of non-atomicity of integer incrementation,
 * showing that such operation should be protected with mutex to be immune from
 * concurrent access.
 *
 * When compiled with macro BUG defined (--cflags="-DBUG"), locking is disabled
 * and therefore assertion violation is detected.
 *
 * Verify for deadlock freedom with:
 *  $ divine compile --llvm [--cflags=" < flags > "] global.c
 *  $ divine verify global.bc -p deadlock [-d]
 *
 * Verify for assertion safety (no assertion violation)
 *  $ divine compile --llvm [--cflags=" < flags > "] global.c
 *  $ divine verify global.bc -p assert [-d]
 *
 * Execute with:
 *  $ clang [ < flags > ] -lpthread -o global.exe global.c
 *  $ ./global.exe
 */

#include <pthread.h>
#include "stdlib.h"

// For native execution (in future we will provide cassert).
#ifndef DIVINE
#include "assert.h"
#endif

int i = 33;

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
