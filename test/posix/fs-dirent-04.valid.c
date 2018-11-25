/* TAGS: c */
#include <assert.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>

int main() {
    errno = 0;
    DIR *d = opendir( "dir" );
    assert( d == NULL );
    assert( errno == ENOENT );
    closedir( d );
    return 0;
}
