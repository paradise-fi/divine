/* TAGS: c */
#include <unistd.h>
#include <assert.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdbool.h>
#include <errno.h>
#include <stdio.h>

bool exists( const char *fn )
{
    return access( fn, F_OK ) == 0; // -1 && errno == ENOENT;
}

int main() {
    int fd = open( "file", O_CREAT | O_WRONLY, 0644 );
    assert( fd >= 0 );
    assert( close( fd ) == 0 );

    /* file to nonexistent */
    assert( exists( "file" ) );
    assert( !exists( "renamed" ) );
    assert( rename( "file", "renamed" ) == 0 );
    assert( !exists( "file" ) );
    assert( exists( "renamed" ) );

    /* file to dir */
    assert( mkdir( "dir", 0755 ) == 0 );
    assert( rename( "renamed", "dir" ) == -1 );
    assert( errno == EISDIR );

    /* dir to file */
    assert( exists( "dir" ) );
    assert( rename( "dir", "renamed" ) == -1 );
    assert( errno == ENOTDIR );
    assert( exists( "dir" ) );

    /* dir to empty dir */
    assert( mkdir( "dir2", 0755 ) == 0 );
    assert( rename( "dir", "dir2" ) == 0 );
    assert( !exists( "dir" ) );
    assert( exists( "dir2" ) );

    /* dir to nonempty dir */
    assert( mkdir( "dir", 0755 ) == 0 );
    assert( mkdir( "dir/x", 0755 ) == 0 );
    assert( rename( "dir2", "dir" ) == -1 );
    assert( errno == ENOTEMPTY );
    assert( exists( "dir" ) );
    assert( exists( "dir2" ) );

    /* nonempty dir to nonexistent */
    assert( rename( "dir", "dir3" ) == 0 );
    assert( !exists( "dir" ) );
    assert( exists( "dir2" ) );
    assert( exists( "dir3" ) );

    /* nonempty dir to empty dir */
    assert( rename( "dir3", "dir2" ) == 0 );
    assert( exists( "dir2/x" ) );

    return 0;
}
