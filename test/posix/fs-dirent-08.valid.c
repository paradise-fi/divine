/* TAGS: c */
#include <assert.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>

int main() {
    errno = 0;
    assert( closedir( NULL ) == -1 );
    assert( errno == EBADF );
    return 0;
}
