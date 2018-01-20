/* TAGS: c */
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

int main() {
    int fd = open( "file", O_CREAT | O_WRONLY, 0644 );
    assert( fd >= 0 );
    assert( close( fd ) == 0 );

    assert( rename( "file", "renamed" ) == 0 );

    errno = 0;
    assert( access( "file", F_OK ) == -1 );
    assert( errno == ENOENT );

    int r = access( "renamed", F_OK );
    assert( r == 0 );

    return 0;
}
