/* TAGS: c */
#include <unistd.h>
#include <sys/stat.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>

int main() {
    assert( mkdir( "dir", 0755 ) == 0 );
    assert( access( "dir", X_OK | W_OK | R_OK ) == 0 );
    int fd = open( "dir/file", O_CREAT | O_WRONLY, 0644 );
    assert( fd >= 0 );
    assert( close( fd ) == 0 );

    assert( symlink( "/", "dir/linkA" ) == 0 );
    assert( symlink( "../", "dir/linkB" ) == 0 );
    assert( symlink( "./..", "dir/linkC" ) == 0 );

    assert( access( "dir/file", W_OK | R_OK ) == 0 );
    assert( access( "dir/linkA/dir/file", W_OK | R_OK ) == 0 );
    assert( access( "dir/linkB/dir/file", W_OK | R_OK ) == 0 );
    assert( access( "dir/linkC/dir/file", W_OK | R_OK ) == 0 );

    assert( access( "dir/linkA/dir/linkA/dir/file", F_OK ) == 0 );

    assert( mkdir( "dir/linkA/dir2", 0755 ) == 0 );
    assert( access( "dir2", F_OK ) == 0 );
    assert( access( "dir/linkA/dir2", F_OK ) == 0 );
    assert( access( "dir/linkB/dir2", F_OK ) == 0 );
    assert( access( "dir/linkC/dir2", F_OK ) == 0 );

    return 0;
}
