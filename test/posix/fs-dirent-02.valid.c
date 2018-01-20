/* TAGS: c */
#include <assert.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

int main() {
    int fd = open( "file", O_CREAT | O_WRONLY, 0644 );
    assert( fd >= 0 );
    assert( close( fd ) == 0 );

    errno = 0;
    DIR *d = opendir( "file" );
    assert( d == NULL );
    assert( errno == ENOTDIR );
    return 0;
}
