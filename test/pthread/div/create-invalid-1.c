/* TAGS: threads c */
#include <pthread.h>

void *thread( void *x )
{
    return x;
}

int main( int argc, char * argv[] )
{
    pthread_create( NULL, NULL, thread, NULL ); /* ERROR */
    return 0;
}
