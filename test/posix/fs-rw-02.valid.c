/* TAGS: c */
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>

int main() {
    char buf[8] = {};
    int fd = open( "test", O_WRONLY | O_CREAT, 0644  );
    assert( fd >= 0 );
    assert( read( fd, buf, 7 ) == -1 );
    assert( errno == EBADF );
    assert( close( fd ) == 0 );
    errno = 0;
    assert( write( fd, "x", 1 ) == -1 );
    assert( errno == EBADF );
    return 0;
}
