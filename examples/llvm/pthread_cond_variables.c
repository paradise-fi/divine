/*
 * Pthread conditional variables
 * =============================
 *
 *  Simulation of the producer-consument(s) pattern, implemented using conditional
 *  variables.
 *
 *  *tags*: test, C99
 *
 * Description
 * -----------
 *
 *  This program simulates the *producer-consument(s) pattern*, implemented using
 *  conditional variables. Instead of spinning, consumers are sleeping
 *  until some item is enqueued into the shared queue.
 *  If compiled with `-DBUG`, condition is not re-checked after the
 *  `pthread_cond_wait` returns. But this is obviously incorrect because
 *  when a thread receives a signal it is not necessarily the first one that
 *  gets scheduled when the signalling thread unlocks the mutex.
 *  For example we could have the following series of operations:
 *
 *    1. *Cons_0* locks queue_mutex, sees that `enqueued == 0`, and waits (releases mutex).
 *    2. *Prod* locks queue_mutex, produces new item and signals about new item being added.
 *    3. *Cons_1* tries to lock `queue_mutex`, but fails and has to wait.
 *    4. *Prod* unclocks `queue_mutex` and the scheduler decides to schedule *Cons_1*
         instead of *Cons_0*.
 *    5. *Cons_1* locks `queue_mutex`, sees that `enqueued != 0`, and removes the item
 *       (but note that *Prod* already signalled *Cons_0* that it should get this item).
 *    6. *Cons_1* unlocks queue_mutex and the scheduler resumes *Cons_0*.
 *    7. *Cons_0* now tries to remove from an empty queue.
 *
 *  <!-- -->
 *
 *  Thus at least two consumers and two items to process are needed to have
 *  assertion violated in some run of the program.
 *
 *  Current implementation of conditional variables in Pthread library
 *  provided within DiVinE also takes into consideration that `pthread_cond_signal`
 *  can send signal to more than one thread (as it is defined in the *POSIX* specifications).
 *  However spurious wakeup is not yet implemented.
 *
 * Parameters
 * ----------
 *
 *  - `BUG`: if defined than the algorithm is incorrect and violates the safety property
 *  - `NUM_OF_CONSUMERS`: the number of consumers
 *  - `NUM_OF_ITEMS_TO_PROCESS`: the number of items to be processed by consumers
 *
 * Verification
 * ------------
 *
 *  - all available properties with the default values of parameters:
 *
 *         $ divine compile --llvm pthread_cond_variables.c
 *         $ divine verify -p assert pthread_cond_variables.bc -d
 *         $ divine verify -p deadlock pthread_cond_variables.bc -d
 *
 *  - playing with the parameters:
 *
 *         $ divine compile --llvm --cflags="-DNUM_OF_CONSUMERS=4 -DNUM_OF_ITEMS_TO_PROCESS=8" pthread_cond_variables.c
 *         $ divine verify -p assert pthread_cond_variables.bc -d
 *         $ divine verify -p deadlock pthread_cond_variables.bc -d
 *
 * Execution
 * ---------
 *
 *       $ clang -lpthread -o pthread_cond_variables.exe pthread_cond_variables.c
 *       $ ./pthread_cond_variables.exe
 */

#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <stdio.h>

#ifndef NUM_OF_CONSUMERS
#define NUM_OF_CONSUMERS         2
#endif

#ifndef NUM_OF_ITEMS_TO_PROCESS
#define NUM_OF_ITEMS_TO_PROCESS  2
#endif

volatile int enqueued = 0;
volatile int dequeued = 0;

pthread_mutex_t queue_mutex;
pthread_cond_t queue_emptiness_cv;

void *consumer( void *arg )
{
#ifndef __divine__
    intptr_t id = ( intptr_t ) arg;
#endif
    while ( 1 ) {

        pthread_mutex_lock( &queue_mutex );
        
#ifdef BUG
        if ( dequeued < NUM_OF_ITEMS_TO_PROCESS && enqueued == 0 ) {
#else // correct
        while ( dequeued < NUM_OF_ITEMS_TO_PROCESS && enqueued == 0 ) {
#endif
#ifndef __divine__
            printf( "Consumer ID = %d is going to sleep.\n", id );
#endif
            pthread_cond_wait( &queue_emptiness_cv, &queue_mutex );
#ifndef __divine__
            printf( "Consumer ID = %d was woken up.\n", id );
#endif
        }

        if ( dequeued == NUM_OF_ITEMS_TO_PROCESS ) {
            pthread_mutex_unlock( &queue_mutex );
            break;
        } 

        // dequeue
        assert( enqueued > 0 ); // fails if macro BUG is defined
        ++dequeued;
        --enqueued;
#ifndef __divine__
        printf( "Consumer ID = %d dequeued an item.\n", id );
#endif

        pthread_mutex_unlock( &queue_mutex );

        // here the thread would do some work with dequeued item (in his local space)
#ifndef __divine__
        sleep( 1 );
#endif
    }
    return NULL;
}

int main( void )
{
  int i;
  pthread_t * threads = ( pthread_t * )( malloc( sizeof( pthread_t ) * NUM_OF_CONSUMERS ) );
  if ( !threads )
      return 1;

  // initialize mutex and condition variable objects
  pthread_mutex_init( &queue_mutex, NULL );
  pthread_cond_init ( &queue_emptiness_cv, NULL );

  // create and start all consumers
  for ( i=0; i<NUM_OF_CONSUMERS; i++ ) {
      pthread_create( &threads[i], 0, consumer, ( void* )( intptr_t )( i+1 ) );
  }

  // producer:
  for ( i=0; i<NUM_OF_ITEMS_TO_PROCESS; i++ ) {
      // here the main thread would produce some item to process
#ifndef __divine__
      sleep(1);
#endif

      pthread_mutex_lock( &queue_mutex );

      //enqueue
      enqueued++;

      // wake up at least one consumer
      pthread_cond_signal( &queue_emptiness_cv );

      pthread_mutex_unlock( &queue_mutex );
  }

  // when it is finnished, wake up all consumers
  pthread_cond_broadcast( &queue_emptiness_cv );

  // wait for all threads to complete
  for ( i=0; i<NUM_OF_CONSUMERS; i++ ) {
      pthread_join( threads[i], NULL );
  }

  assert( enqueued == 0 );
  assert( dequeued == NUM_OF_ITEMS_TO_PROCESS );

  // clean up and exit
  pthread_mutex_destroy( &queue_mutex );
  pthread_cond_destroy ( &queue_emptiness_cv );
  free( threads );

  return 0;
}
