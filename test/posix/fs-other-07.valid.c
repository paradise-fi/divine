/* TAGS: c */
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

int main() {
    int fd = open( "file", O_CREAT | O_WRONLY, 0644 );
    assert( fd >= 0 );

    errno = 0;
    assert( fchdir( fd ) == -1 );
    assert( errno == ENOTDIR );

    assert( close( fd ) == 0 );


    errno = 0;
    assert( fchdir( 20 ) == -1 );
    assert( errno == EBADF );

    assert( mkdir( "dir", 0644 ) == 0 );
    fd = open( "dir", O_RDONLY );
    errno = 0;
    assert( fchdir( fd ) == -1 );
    assert( errno == EACCES );

    assert( close( fd ) == 0 );

    return 0;
}
