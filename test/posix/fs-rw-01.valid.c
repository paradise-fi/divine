/* TAGS: min c */
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

int main() {
    char buf[8] = {};
    int fd = open( "test", O_WRONLY | O_CREAT, 0644 );
    assert( fd >= 0 );
    assert( write( fd, "tralala", 7 ) == 7 );
    assert( close( fd ) == 0 );
    fd = open( "test", O_RDONLY );
    assert( fd >= 0 );
    assert( read( fd, buf, 7 ) == 7 );
    assert( strcmp( "tralala", buf ) == 0 );
    assert( close( fd ) == 0 );
    return 0;
}
