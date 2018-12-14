# TAGS: min
. lib/testcase

mkdir -p capture

cat > capture/fs-symlink-capture.c <<EOF
/* VERIFY_OPTS: --capture capture/link:follow:/ */
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <string.h>
#include <errno.h>

int main() {
    int fd = open( "file", O_WRONLY );
    assert( fd >= 0 );
    assert( close( fd ) == 0 );

    assert( access( "file", W_OK | R_OK ) == 0 );
    assert( access( "symlinkFile", W_OK | R_OK ) == 0 );

    fd = unlink( "file" );
    assert( fd == 0 );

    assert( access( "symlinkFile",  W_OK | R_OK ) == -1 );
}
EOF

mkdir capture/link
mkdir capture/link/dir
touch capture/link/file
ln capture/link/file capture/link/hardlinkFile
cd capture/link
ln -s file symlinkFile
cd ../..

verify capture/fs-symlink-capture.c
