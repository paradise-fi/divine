/* TAGS: c */
#include <unistd.h>
#include <sys/stat.h>
#include <assert.h>
#include <errno.h>

int main() {
    assert( mkdir( "dir", 0755 ) == 0 );
    errno = 0;
    assert( link( "dir", "link" ) == -1 );
    assert( errno == EPERM );
    return 0;
}
