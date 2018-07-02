/* TAGS: c */
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

int main() {
    char buf[8]={};
    assert( mkdir( "dir", 0755 ) == 0 );
    int fd = open( "dir", O_RDONLY );
    assert( fd >= 0 );
    assert( read( fd, buf, 7 ) == -1 );
    assert( errno == EISDIR );
    assert( close( fd ) == 0 );
    return 0;
}
