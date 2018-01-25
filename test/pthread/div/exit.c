/* TAGS: min threads c */
#include <assert.h>
#include <pthread.h>

void *thread( void *x ) {
     pthread_exit( 1 );
     return 0;
}

int main() {
     pthread_t tid;
     pthread_create( &tid, NULL, thread, NULL );
     void *i = 0;
     pthread_join( tid, &i );
     assert( i == 1 );
     return 0;
}
