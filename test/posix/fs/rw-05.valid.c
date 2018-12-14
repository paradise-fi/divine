/* TAGS: c */
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

int main() {
    char buf[8]={};
    int fd = open( "test", O_WRONLY | O_CREAT, 0644 );
    assert( fd >= 0 );
    assert( write( fd, "tralala", 7 ) == 7 );
    assert( close( fd ) == 0 );

    fd = open( "test", O_RDWR );
    assert( fd >= 0 );
    assert( read( fd, buf, 7 ) == 7 );
    assert( strcmp( buf, "tralala" ) == 0 );
    assert( pwrite( fd, "h", 1, 0 ) == 1 );
    assert( pwrite( fd, "b", 1, 3 ) == 1 );
    assert( pread( fd, buf, 7, 0 ) == 7 );
    assert( strcmp( buf, "hrabala" ) == 0 );
    assert( close( fd ) == 0 );
    return 0;
}
