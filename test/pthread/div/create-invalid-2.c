/* TAGS: threads c */
/* EXPECT: --result error --symbol __pthread_start */
#include <pthread.h>

int main( int argc, char * argv[] )
{
    pthread_t tid;
    pthread_create( &tid, NULL, NULL, NULL );
    return 0;
}
