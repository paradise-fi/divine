/*
 * Pthread library -- showcase
 * ===========================
 *
 *  Showcase of some Pthread features already interpreted by DiVinE.
 *
 *  *tags*: test, C++98
 *
 * Description
 * -----------
 *
 *  This program is just a showcase (suitable for testing) of some Pthread features
 *  already interpreted by DiVinE. Verification should proceed without detecting
 *  any safety violation.
 *
 * Parameters
 * ----------
 *
 *  - `NUM_OF_THREADS`: the number of threads to be created (other than the main thread)
 *
 * Verification
 * ------------
 *
 *  - all available properties with the default values of parameters:
 *
 *         $ divine compile --llvm pthread_showcase.cpp
 *         $ divine verify -p assert pthread_showcase.bc -d
 *         $ divine verify -p safety pthread_showcase.bc -d
 *
 *  - changing the number of threads:
 *
 *         $ divine compile --llvm --cflags="-DNUM_OF_THREADS=4" pthread-showcase.cpp
 *         $ divine verify -p assert pthread-showcase.bc -d
 *
 * Execution
 * ---------
 *
 *       $ clang++ -lpthread -o pthread-showcase.exe pthread-showcase.cpp
 *       $ ./pthread-showcase.exe
 */

#include <pthread.h>
#include <assert.h>
#include <stdlib.h>
#include <errno.h>

#ifndef NUM_OF_THREADS
#define NUM_OF_THREADS 2
#endif

#ifdef DEBUG
#ifndef __divine__  // native execution + debug
#include <iostream>
#define PRINT_ERROR( x ) std::cout << x << " at line " << __LINE__ << std::endl
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
#else               // verification + debug
#define ERRNO( fun ) assert( !fun );
#endif
#else               // verification or native execution, but debug is disabled
#define ERRNO( fun ) fun;
#endif

pthread_mutex_t counter_mutex;
pthread_cond_t counter_cond;
volatile int counter = 0;

pthread_key_t key[2];

pthread_once_t once[] = { PTHREAD_ONCE_INIT, PTHREAD_ONCE_INIT };
volatile bool once_triggered[2] = { false, false };
pthread_mutex_t once_mutex[2];

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

void* thread( void * arg ) {
  /* once-only execution */
    ERRNO( pthread_once( &once[0], init_key<0> ) )
    ERRNO( pthread_once( &once[1], init_key<1> ) )

  /* conditional variables (barrier) */
    ERRNO( pthread_mutex_lock( &counter_mutex ) )
    ++counter;
    if ( counter == NUM_OF_THREADS )
        ERRNO( pthread_cond_broadcast( &counter_cond ) )
    while ( counter < NUM_OF_THREADS ) {
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
    pthread_t t[NUM_OF_THREADS];
    ERRNO( pthread_mutex_init( &counter_mutex, NULL ) )
    ERRNO( pthread_mutex_init( &once_mutex[0], NULL ) )
    ERRNO( pthread_mutex_init( &once_mutex[1], NULL ) )
    ERRNO( pthread_cond_init( &counter_cond, NULL ) )

  /* pthread_create */
    for ( int i = 0; i < NUM_OF_THREADS; i++ ) {
        ERRNO( pthread_create( &t[i], 0, thread, reinterpret_cast<void*>( i+1 ) ) )

#ifdef __divine__
        // Barrier implemented in the thread entry function ensures that thread IDs
        // are not recycled. But this is specific to the implementation provided by DiVinE.
        assert( t[i] == ( ( (i+1) << 16) | (i+1) ) );
#endif
    }

  /* pthread_join */
    void* result;
    for ( int i = 0; i < NUM_OF_THREADS; i++ ) {
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
