# TAGS: min capture
. lib/testcase

mkdir -p capture

cat > capture/fs-open-capture.c <<EOF
/* VERIFY_OPTS: --capture capture/link:follow:/ */
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>

int main() {

    int fd = open( "dir", O_RDONLY );
    assert( fd >= 0 );
    assert( close( fd ) == 0 );

    fd = open( "dir", O_WRONLY );
    assert( fd == -1 );
    assert( errno == EISDIR );
    return 0;
}
EOF

mkdir capture/link
mkdir capture/link/dir
touch capture/link/file
ln capture/link/file capture/link/hardlinkFile

verify capture/fs-open-capture.c
