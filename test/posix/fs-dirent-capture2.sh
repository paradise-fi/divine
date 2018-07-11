# TAGS: min
. lib/testcase

mkdir -p capture

cat > capture/fs-dirent-capture.c <<EOF
/* VERIFY_OPTS: --capture capture/dir:follow:/ */
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>

int main() {

    int fd = open( "emptyDir", O_RDONLY );
    assert( fd >= 0 );
    assert( close( fd ) == 0 );

    fd = open( "emptyDir", O_WRONLY );
    assert( fd == -1 );
    assert( errno == EISDIR );
    return 0;
}
EOF

mkdir capture/dir
mkdir capture/dir/emptyDir

verify capture/fs-dirent-capture.c
