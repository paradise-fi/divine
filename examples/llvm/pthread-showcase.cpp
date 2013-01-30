/*
 * This program is just a showcase (suitable for testing) of some pthread features
 * already interpreted by DiVinE. Verification should proceed without detecting
 * any safety violation.
 *
 * Verify with:
 *  $ divine compile --llvm [--cflags=" < flags > "] pthread-showcase.cpp
 *  $ divine pthread-showcase.bc -p assert [-d]
 * Execute with:
 *  $ clang++ [ < flags > ] -lpthread -o pthread-showcase.exe pthread-showcase.cpp
 *  $ ./pthread-showcase.exe
 */

#include <pthread.h>

// For native execution (in future we will provide cassert).
#ifndef DIVINE
#include <cassert>
#include <cstdlib>
#endif

#define THREADS 2

#ifdef DEBUG
#ifdef TRACE
#include <errno.h>
#define PRINT_ERROR( x ) trace( x " at line %d", __LINE__ )
#define ERRNO( fun ) do { int ec = fun;                                        \
                          switch ( ec ) {                                      \
                              case EINVAL: PRINT_ERROR( "EINVAL" ); break;     \
                              case EBUSY: PRINT_ERROR( "EBUSY" ); break;       \
                              case EPERM: PRINT_ERROR( "EPERM" ); break;       \
                              case ESRCH: PRINT_ERROR( "ESRCH" ); break;       \
                              case EAGAIN: PRINT_ERROR( "EAGAIN" ); break;     \
                              case EDEADLK: PRINT_ERROR( "EDEADLK" ); break;   \
                          }                                                    \
                        assert( !ec ); } while( 0 );
#else // trace is not available
#define ERRNO( fun ) assert( !fun );
#endif
#else // debug is disabled
#define ERRNO( fun ) fun;
#endif

pthread_mutex_t counter_mutex;
pthread_cond_t counter_cond;
volatile int counter = 0;

pthread_key_t key[2];

pthread_once_t once[] = { PTHREAD_ONCE_INIT, PTHREAD_ONCE_INIT };
volatile bool once_triggered[2] = { false, false };
pthread_mutex_t once_mutex[2];

#ifndef NEW_INTERP_BUGS
template< int idx >
void init_key( void ) {
   /* verify correctness of pthread_once implementation */
    ERRNO( pthread_mutex_lock( &once_mutex[idx] ) )
    assert( !once_triggered[idx] );
    once_triggered[idx] = true;
    ERRNO( pthread_mutex_unlock( &once_mutex[idx] ) )

   /* allocation of TLS */
    ERRNO( pthread_key_create( &key[idx], NULL ) )
}
#else
void init_key0( void ) {
   /* verify correctness of pthread_once implementation */
    ERRNO( pthread_mutex_lock( &once_mutex[0] ) )
    assert( !once_triggered[0] );
    once_triggered[0] = true;
    ERRNO( pthread_mutex_unlock( &once_mutex[0] ) )

   /* allocation of TLS */
    ERRNO( pthread_key_create( &key[0], NULL ) )
}

void init_key1( void ) {
   /* verify correctness of pthread_once implementation */
    ERRNO( pthread_mutex_lock( &once_mutex[1] ) )
    assert( !once_triggered[1] );
    once_triggered[1] = true;
    ERRNO( pthread_mutex_unlock( &once_mutex[1] ) )

   /* allocation of TLS */
    ERRNO( pthread_key_create( &key[1], NULL ) )
}
#endif

void* thread( void * arg ) {
  /* once-only execution */
#ifndef NEW_INTERP_BUGS
    ERRNO( pthread_once( &once[0], init_key<0> ) )
    ERRNO( pthread_once( &once[1], init_key<1> ) )
#else
    ERRNO( pthread_once( &once[0], init_key0 ) )
    ERRNO( pthread_once( &once[1], init_key1 ) )
#endif

  /* conditional variables (barrier) */
    ERRNO( pthread_mutex_lock( &counter_mutex ) )
    ++counter;
    if ( counter == THREADS )
        ERRNO( pthread_cond_broadcast( &counter_cond ) )
    while ( counter < THREADS ) {
        ERRNO( pthread_cond_wait( &counter_cond, &counter_mutex ) )
    }
    ERRNO( pthread_mutex_unlock( &counter_mutex ) )

  /* TLS */
    ERRNO( pthread_setspecific( key[0],arg ) )
    long int x = reinterpret_cast<long int>( pthread_getspecific( key[0] ) );
    ERRNO( pthread_setspecific( key[1], reinterpret_cast<void*>( x * 2 ) ) )

    return pthread_getspecific( key[1] );
}

int main( void ) {
  /* init */
    pthread_t t[THREADS];
    ERRNO( pthread_mutex_init( &counter_mutex, NULL ) )
    ERRNO( pthread_mutex_init( &once_mutex[0], NULL ) )
    ERRNO( pthread_mutex_init( &once_mutex[1], NULL ) )
    ERRNO( pthread_cond_init( &counter_cond, NULL ) )

  /* pthread_create */
    for ( int i = 0; i < THREADS; i++ ) {
        ERRNO( pthread_create( &t[i], 0, thread, reinterpret_cast<void*>( i+1 ) ) )

        // Barrier implemented in the thread entry function ensures that thread IDs
        // are not recycled.
#ifdef DIVINE
        assert( t[i] == ( ( (i+1) << 16) | (i+1) ) );
#endif
    }

  /* pthread_join */
    void* result;
    for ( int i = 0; i < THREADS; i++ ) {
        ERRNO( pthread_join( t[i], &result ) )

        long int _result = reinterpret_cast<long int>( result );
        long int _expected = ( i+1 ) * 2;

        // Barrier implemented in the thread entry function ensures that storage used
        // for results do not overlap.
        assert( _result == _expected );
    }

  /* clean up and exit */
    ERRNO( pthread_mutex_destroy( &counter_mutex ) )
    ERRNO( pthread_mutex_destroy( &once_mutex[0] ) )
    ERRNO( pthread_mutex_destroy( &once_mutex[1] ) )
    ERRNO( pthread_cond_destroy( &counter_cond ) )
    ERRNO( pthread_key_delete( key[0] ) )
    ERRNO( pthread_key_delete( key[1] ) )

    return 0;
}
