/* TAGS: c */
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

int main() {
    errno = 0;
    assert( ftruncate( 20, 0 ) == -1 );
    assert( errno == EBADF );

    assert( mkdir( "foo", 0755 ) == 0 );
    int fd = open( "foo", O_RDONLY );
    assert( fd >= 0 );
    errno = 0;
    assert( ftruncate( fd, 0 ) == -1 );
    assert( errno == EINVAL );
    assert( close( fd ) == 0 );

    errno = 0;
    assert( truncate( "foo", 0 ) == -1 );
    assert( errno == EISDIR );

    errno = 0;
    assert( truncate( "nonexistent", 0 ) == -1 );
    assert( errno == ENOENT );
    return 0;
}
