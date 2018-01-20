/* TAGS: ext c */
#include <pthread.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>

const char *message = "1234567";

void *worker( void *_ ) {
    int fd = open( "pipe", O_WRONLY );
    assert( fd >= 0 );

    assert( write( fd, message, 7 ) == 7 );
    assert( write( fd, "-", 1 ) == 1 );

    assert( close( fd ) == 0 );
    return _;
}

int main() {
    char buf[ 8 ] = {};
    assert( mkfifo( "pipe", 0644 ) == 0 );

    pthread_t thread;
    pthread_create( &thread, NULL, worker, NULL );

    int fd = open( "pipe", O_RDONLY );
    assert( fd >= 0 );
    char incoming;
    for ( int i = 0; ; ++i ) {
        assert( read( fd, &incoming, 1 ) == 1 );
        if ( incoming == '-' )
            break;
        buf[ i ] = incoming;
    }

    pthread_join( thread, NULL );

    assert( strcmp( buf, message ) == 0 );

    return 0;
}
