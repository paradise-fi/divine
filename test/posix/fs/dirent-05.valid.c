/* TAGS: c */
#include <assert.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>

int main() {
    errno = 0;
    DIR *d = fdopendir( 20 );
    assert( d == NULL );
    assert( errno == EBADF );
    closedir( d );
    return 0;
}
