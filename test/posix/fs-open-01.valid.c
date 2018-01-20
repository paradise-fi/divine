/* TAGS: min c */
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>

int main() {
    int fd = open( "test", O_RDONLY );
    assert( fd == -1 );
    assert( errno == ENOENT );
    return 0;
}
