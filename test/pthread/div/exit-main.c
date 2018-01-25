/* TAGS: min threads c */
#include <assert.h>
#include <pthread.h>

void *thread( void *x ) {
     return 0;
}

int main() {
     pthread_t tid;
     pthread_create( &tid, NULL, thread, NULL );
     pthread_join( tid, NULL );
     int ret = 1;
     pthread_exit( &ret );
     return 0;
}
