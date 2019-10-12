/* TAGS: threads c */
#include <pthread.h>

int main( int argc, char * argv[] )
{
    pthread_create( NULL, NULL, NULL, NULL ); /* ERROR */
    return 0;
}
