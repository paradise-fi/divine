#include <assert.h>
#include <pthread.h>

int shared = 0;
pthread_mutex_t mutex;

void *thread( void *x )
 {
     pthread_mutex_lock( &mutex );
     ++ shared;
     pthread_mutex_unlock( &mutex );
}

int main()
 {
     pthread_t tid;
     pthread_mutex_init( &mutex, NULL );
     pthread_create( &tid, NULL, thread, NULL );
     pthread_mutex_lock( &mutex );
     ++ shared;
     pthread_mutex_unlock( &mutex );
     pthread_join( tid, NULL );
     assert( shared == 2 );
     return 0;
}
