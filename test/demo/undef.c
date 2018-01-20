/* TAGS: min c */
#include "demo.h"

void *thread( void *state ) {
    puts( "thread is running" );
    int *x = state;
    if ( *x == 0 ) /* ERROR */
        puts( "x is zero" );
    else if ( *x == 1 )
        puts( "x is one" );
    else
        puts( "x is neither zero nor one" );
    puts( "thread is done" );
    return NULL;
}

int main() {
    puts( "entering main" );
    pthread_t tid;
    int x;
    puts( "going to create thread" );
    pthread_create( &tid, NULL, thread, &x );
    puts( "thread created" );
    x = 1;
    puts( "initialized x, going to wait for the thread" );
    pthread_join( tid, NULL );
    puts( "thread finished, exiting" );
}
