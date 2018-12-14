/* TAGS: c */
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

int main() {

    int fd = open( "file", O_CREAT | O_RDWR, 0644 );
    assert( fd >= 0 );

    assert( write( fd, "tralala", 7 ) == 7 );

    assert( lseek( fd, 0, SEEK_CUR ) == 7 );
    assert( lseek( fd, -7, SEEK_CUR ) == 0 );
    assert( lseek( fd, -5, SEEK_END ) == 2 );
    assert( lseek( fd, 5, SEEK_END ) == 7 + 5 );

    errno = 0;
    assert( lseek( fd, -1, SEEK_SET ) == -1 );
    assert( errno == EINVAL );

    errno = 0;
    assert( lseek( fd, -8, SEEK_END ) == -1 );
    assert( errno == EINVAL );

    assert( lseek( fd, 2, SEEK_SET ) == 2 );
    errno = 0;
    assert( lseek( fd, -3, SEEK_CUR ) == -1 );
    assert( errno == EINVAL );

    errno = 0;
    assert( lseek( fd, 0, 20 ) == -1 );
    assert( errno == EINVAL );

    assert( close( fd ) == 0 );
    return 0;
}
