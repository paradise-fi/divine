/* TAGS: c */
#include <unistd.h>
#include <sys/stat.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>

int main() {
    char buf[8] = {};
    int fd = open( "file", O_CREAT | O_WRONLY, 0644 );
    assert( fd >= 0 );
    assert( close( fd ) == 0 );

    assert( symlink( "file", "link" ) == 0 );
    assert( readlink( "link", buf, 8 ) == 4 );
    assert( strcmp( buf, "file" ) == 0 );

    fd = open( "link", O_WRONLY );
    assert( fd >= 0 );
    assert( write( fd, "tralala", 7 ) == 7 );
    assert( close( fd ) == 0 );

    fd = open( "file", O_RDONLY );
    assert( fd >= 0 );
    assert( read( fd, buf, 7 ) == 7 );
    assert( close( fd ) == 0 );

    assert( strcmp( buf, "tralala" ) == 0 );
    assert( unlink( "link" ) == 0 );
    assert( access( "file", W_OK | R_OK ) == 0 );
    return 0;
}
