/* TAGS: min c */
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>

pthread_mutex_t mutex;
volatile char shared;

void *thread( void * unused )
 {
     ( void ) unused;
     pthread_mutex_lock( &mutex );
     shared |= 0x01;
     pthread_mutex_unlock( &mutex );
     return NULL;
}

int main() {
    int undef;
    shared = undef;
    shared &= 0xF0;

    pthread_t tid;
    pthread_mutex_init( &mutex, NULL );
    pthread_create( &tid, NULL, thread, NULL );
    pthread_mutex_lock( &mutex );
    shared |= 0x04;
    pthread_mutex_unlock( &mutex );
    pthread_join( tid, NULL );

    assert( ( shared & 0x06 ) == 0x04 );
    assert( shared == 0x05 ); /* ERROR */

    return 0;
}
