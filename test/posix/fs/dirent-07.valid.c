/* TAGS: c */
#include <assert.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>

int main() {
    errno = 0;
    struct dirent *item = readdir( NULL );
    assert( item == NULL );
    assert( errno == EBADF );
    return 0;
}
