. lib/testcase

mkdir -p capture

cat > capture/fs-grants-capture.c <<EOF

#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>

int main() {

    int fd = open( "readOnly", O_RDONLY );
    assert( fd >= 0 );
    assert( close( fd ) == 0 );

    fd = open( "writeOnly", O_WRONLY );
    assert( fd >= 0 );
    assert( close( fd ) == 0 );

    fd = open( "readOnly", O_WRONLY );
    assert( fd == -1 );
    assert( errno == EACCES );

    fd = open( "writeOnly", O_RDONLY );
    assert( fd == -1 );
    assert( errno == EACCES );

    fd = open( "noAccess", O_WRONLY );
    assert( fd == -1 );
    assert( errno == EACCES );

    fd = open( "noAccess", O_RDONLY );
    assert( fd == -1 );
    assert( errno == EACCES );

    fd = unlink( "noAccess" );
    assert( fd == -1 );
    assert( errno == EACCES );
    return 0;
}
EOF

mkdir capture/grants
touch capture/grants/noAccess
touch capture/grants/writeOnly
touch capture/grants/readOnly
chmod -rwx capture/grants/noAccess
chmod -rx capture/grants/writeOnly
chmod -wx capture/grants/readOnly

divine verify --threads 1 --capture capture/grants:follow:/ capture/fs-grants-capture.c

