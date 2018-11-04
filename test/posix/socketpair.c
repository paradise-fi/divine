/* TAGS: c */
#include <pthread.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>

int writer;
int reader;

void *worker( void *_ )
{
    (void)_;
    char buf[ 6 ] = {};
    int len = read( reader, buf, 5 );
    if ( len < 0 )
        __dios_trace_f( "errno = %d", errno );
    assert( len == 5 );
    __dios_trace_f( "BUF: '%s'", buf );
    assert( strcmp( buf, "12345" ) == 0 );

    for ( int i = 0; i < 5; ++i )
        assert( read( reader, buf + i, 1 ) == 1 );
    assert( strcmp( buf, "67890" ) == 0 );
    return NULL;
}

int main()
{
    pthread_t thread;
    int pfd[ 2 ];

    assert( socketpair( AF_UNIX, SOCK_STREAM, 0, pfd ) == 0 );
    reader = pfd[ 0 ];
    writer = pfd[ 1 ];

    pthread_create( &thread, NULL, worker, NULL );

    assert( write( writer, "1234567", 7 ) == 7 );
    assert( write( writer, "890", 3 ) == 3 );

    pthread_join( thread, NULL );

    assert( close( reader ) == 0 );
    assert( close( writer ) == 0 );
    return 0;
}
