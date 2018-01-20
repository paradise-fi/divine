/* TAGS: c */
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

int main() {
    int fd = open( "file", O_CREAT | O_WRONLY, 0644 );
    assert( fd >= 0 );
    assert( close( fd ) == 0 );

    errno = 0;
    assert( chdir( "file" ) == -1 );
    assert( errno == ENOTDIR );

    errno = 0;
    assert( chdir( "nonexistent" ) == -1 );
    assert( errno == ENOENT );

    assert( mkdir( "dir", 0644 ) == 0 );
    errno = 0;
    assert( chdir( "dir" ) == -1 );
    assert( errno == EACCES );

    return 0;
}
