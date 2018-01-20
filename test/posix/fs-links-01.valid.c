/* TAGS: c */
#include <unistd.h>
#include <sys/stat.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>

int main() {
    char buf[ 8 ] = {};
    int fd = open( "file", O_RDONLY | O_CREAT, 0644 );
    assert( fd >= 0 );

    assert( link( "file", "link" ) == 0 );

    errno = 0;
    assert( link( "file", "link" ) == -1 );
    assert( errno == EEXIST );

    int fdLink = open( "link", O_WRONLY );
    assert( fdLink >= 0 );
    assert( write( fdLink, "tralala", 7 ) == 7 );
    assert( close( fdLink ) == 0 );

    assert( read( fd, buf, 7 ) == 7 );
    assert( strcmp( buf, "tralala" ) == 0 );

    memset( buf, 0, 8 );
    assert( close( fd ) == 0 );

    assert( unlink( "file" ) == 0 );
    fd = open( "link", O_RDONLY );
    assert( read( fd, buf, 7 ) == 7 );
    assert( strcmp( buf, "tralala" ) == 0 );

    assert( close( fd ) == 0 );
    return 0;
}
