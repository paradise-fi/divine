/* TAGS: c */
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <string.h>
#include <errno.h>

int main() {
    char buf[ 8 ] = {};
    int fd = open( "file.txt", O_RDWR | O_CREAT, 0644 );
    assert( fd >= 0 );
    assert( write( fd, "tralala", 7 ) == 7 );

    assert( ftruncate( fd, 4 ) == 0 );
    assert( lseek( fd, 0, SEEK_SET ) == 0 );
    assert( read( fd, buf, 7 ) == 4 );
    assert( strcmp( buf, "tral" ) == 0 );

    assert( close( fd ) == 0 );

    errno = 0;
    assert( truncate( "file.txt", -1 ) == -1 );
    assert( errno == EINVAL );
    assert( truncate( "file.txt", 0 ) == 0 );

    memset( buf, 0, 8 );
    fd = open( "file.txt", O_RDONLY );
    assert( fd >= 0 );
    assert( read( fd, buf, 7 ) == 0 );
    assert( strcmp( buf, "" ) == 0 );
    assert( close( fd ) == 0 );
    return 0;
}
