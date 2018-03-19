/* TAGS: min threads c c11 */
/* CC_OPTS: -std=c11 */
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>

atomic_int shared = 0;
pthread_mutex_t mutex;

void *thread( void *x )
{
     ++shared;
}

int main()
 {
     pthread_t tid;
     pthread_create( &tid, NULL, thread, NULL );
     ++shared;
     pthread_join( tid, NULL );
     assert( shared == 2 );
     return 0;
}

