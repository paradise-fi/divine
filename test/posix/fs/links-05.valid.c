/* TAGS: c */
#include <unistd.h>
#include <sys/stat.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>

int main() {
    char buf[ 8 ] = {};

    int fd = open( "file", O_CREAT | O_WRONLY, 0644 );
    assert( fd >= 0 );
    assert( close( fd ) == 0 );

    assert( mkdir( "dir", 0755 ) == 0 );

    errno = 0;
    assert( readlink( "file", buf, 7 ) == -1 );
    assert( errno == EINVAL );

    errno = 0;
    assert( readlink( "dir", buf, 7 ) == -1 );
    assert( errno == EINVAL );

    return 0;
}
