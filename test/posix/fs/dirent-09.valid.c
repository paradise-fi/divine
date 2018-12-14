/* TAGS: c */
#include <pthread.h>
#include <assert.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>

void *worker( void *_ ) {
    (void)_;
    int fDot = 0;
    int f2Dots = 0;
    int fDir = 0;

    DIR *d = opendir( "." );
    assert( d );

    struct dirent entry;
    struct dirent *result;

    while ( 1 ) {
        assert( readdir_r( d, &entry, &result ) == 0 );
        if ( result == NULL )
            break;

        if ( strcmp( entry.d_name, "." ) == 0 ) {
            assert( !fDot );
            fDot = 1;
        }
        else if ( strcmp( entry.d_name, ".." ) == 0 ) {
            assert( !f2Dots );
            f2Dots = 1;
        }
        else if ( strcmp( entry.d_name, "dir" ) == 0 ) {
            assert( !fDir );
            fDir = 1;
        }
        else {
            assert( 0 );
        }
    }

    assert( fDot );
    assert( f2Dots );
    assert( fDir );

    assert( closedir( d ) == 0 );
    return NULL;
}

int main() {
    assert( mkdir( "dir", 0755 ) == 0 );
    pthread_t thread;
    pthread_create( &thread, NULL, worker, NULL );
    worker( NULL );
    pthread_join( thread, NULL );
    return 0;
}
