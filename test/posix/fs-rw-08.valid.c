/* TAGS: c */
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

int main() {
    char buf[ 8 ] = {};
    int fd = open( "file", O_CREAT | O_RDWR, 0644 );
    int dupfd;
    assert( fd >= 0 );

    assert( write( fd, "tralala", 7 ) == 7 );
    dupfd = dup( fd );
    assert( dupfd >= 0 );
    assert( dupfd != fd );

    assert( lseek( dupfd, 0, SEEK_CUR ) == 7 );
    assert( lseek( dupfd, 0, SEEK_SET ) == 0 );
    assert( lseek( fd, 0, SEEK_CUR ) == 0 );

    assert( close( fd ) == 0 );

    assert( read( dupfd, buf, 7 ) == 7 );
    assert( strcmp( buf, "tralala" ) == 0 );

    assert( close( dupfd ) == 0 );
    return 0;
}
