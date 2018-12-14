/* TAGS: c */
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>

int main() {
    int fd = open( "test", O_WRONLY | O_CREAT, 0644 );
    assert( fd >= 0 );
    assert( close( fd ) == 0 );

    fd = open( "test", O_RDONLY );
    assert( write( fd, "x", 1 ) == -1 );
    assert( errno == EBADF );
    assert( close( fd ) == 0 );
    return 0;
}
