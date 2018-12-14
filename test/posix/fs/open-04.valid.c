/* TAGS: c */
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

int main() {
    assert( mkdir( "dir", 0755 ) == 0 );

    int fd = open( "dir", O_RDONLY );
    assert( fd >= 0 );
    assert( close( fd ) == 0 );

    fd = open( "dir", O_WRONLY );
    assert( fd == -1 );
    assert( errno == EISDIR );
    return 0;
}
