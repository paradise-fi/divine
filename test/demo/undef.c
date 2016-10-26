#include "demo.h"

void *thread( void *state ) {
    print( "thread is running" );
    int *x = state;
    if ( *x == 0 ) /* ERROR */
        print( "x is zero" );
    else if ( *x == 1 )
        print( "x is one" );
    else
        print( "x is neither zero nor one" );
    print( "thread is done" );
    return NULL;
}

int main() {
    print( "entering main" );
    pthread_t tid;
    int x;
    print( "going to create thread" );
    pthread_create( &tid, NULL, thread, &x );
    print( "thread created" );
    x = 1;
    print( "initialized x, going to wait for the thread" );
    pthread_join( tid, NULL );
    print( "thread finished, exiting" );
}
