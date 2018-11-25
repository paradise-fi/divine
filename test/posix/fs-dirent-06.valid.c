/* TAGS: c */
#include <assert.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>

int main() {
    errno = 0;
    DIR *d = fdopendir( 1 );
    assert( d == NULL );
    assert( errno == ENOTDIR );
    closedir( d );
    return 0;
}
