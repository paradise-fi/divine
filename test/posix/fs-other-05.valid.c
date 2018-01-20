/* TAGS: c */
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

int main() {
    assert( mkdir( "dir", 0755 ) == 0 );
    assert( chdir( "dir" ) == 0 );
    int fd = open( "file", O_CREAT | O_WRONLY, 0644 );
    assert( fd >= 0 );
    assert( close( fd ) == 0 );

    assert( chdir( ".." ) == 0 );
    assert( access( "dir", X_OK | W_OK | R_OK ) == 0 );
    assert( access( "dir/file", W_OK | R_OK ) == 0 );

    return 0;
}
