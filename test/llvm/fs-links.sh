. lib
. flavour vanilla

llvm_verify valid <<EOF
#include <unistd.h>
#include <sys/stat.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>

int main() {
    char buf[ 8 ] = {};
    int fd = open( "file", O_RDONLY | O_CREAT, 0644 );
    assert( fd >= 0 );

    assert( link( "file", "link" ) == 0 );

    errno = 0;
    assert( link( "file", "link" ) == -1 );
    assert( errno == EEXIST );

    int fdLink = open( "link", O_WRONLY );
    assert( fdLink >= 0 );
    assert( write( fdLink, "tralala", 7 ) == 7 );
    assert( close( fdLink ) == 0 );

    assert( read( fd, buf, 7 ) == 7 );
    assert( strcmp( buf, "tralala" ) == 0 );

    memset( buf, 0, 8 );
    assert( close( fd ) == 0 );

    assert( unlink( "file" ) == 0 );
    fd = open( "link", O_RDONLY );
    assert( read( fd, buf, 7 ) == 7 );
    assert( strcmp( buf, "tralala" ) == 0 );

    assert( close( fd ) == 0 );
    return 0;
}
EOF

llvm_verify valid <<EOF
#include <unistd.h>
#include <sys/stat.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>

int main() {
    char buf[8] = {};
    int fd = open( "file", O_CREAT | O_WRONLY, 0644 );
    assert( fd >= 0 );
    assert( close( fd ) == 0 );

    assert( symlink( "file", "link" ) == 0 );
    assert( readlink( "link", buf, 8 ) == 4 );
    assert( strcmp( buf, "file" ) == 0 );

    fd = open( "link", O_WRONLY );
    assert( fd >= 0 );
    assert( write( fd, "tralala", 7 ) == 7 );
    assert( close( fd ) == 0 );

    fd = open( "file", O_RDONLY );
    assert( fd >= 0 );
    assert( read( fd, buf, 7 ) == 7 );
    assert( close( fd ) == 0 );

    assert( strcmp( buf, "tralala" ) == 0 );
    assert( unlink( "link" ) == 0 );
    assert( access( "file", W_OK | R_OK ) == 0 );
    return 0;
}
EOF

llvm_verify valid <<EOF
#include <unistd.h>
#include <sys/stat.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>

int main() {
    assert( mkdir( "dir", 0755 ) == 0 );
    assert( access( "dir", X_OK | W_OK | R_OK ) == 0 );
    int fd = open( "dir/file", O_CREAT | O_WRONLY, 0644 );
    assert( fd >= 0 );
    assert( close( fd ) == 0 );

    assert( symlink( "/", "dir/linkA" ) == 0 );
    assert( symlink( "../", "dir/linkB" ) == 0 );
    assert( symlink( "./..", "dir/linkC" ) == 0 );

    assert( access( "dir/file", W_OK | R_OK ) == 0 );
    assert( access( "dir/linkA/dir/file", W_OK | R_OK ) == 0 );
    assert( access( "dir/linkB/dir/file", W_OK | R_OK ) == 0 );
    assert( access( "dir/linkC/dir/file", W_OK | R_OK ) == 0 );

    errno = 0;
    assert( access( "dir/linkA/dir/linkA/dir/file", F_OK ) == -1 );
    assert( errno == ELOOP );

    assert( mkdir( "dir/linkA/dir2", 0755 ) == 0 );
    assert( access( "dir2", F_OK ) == 0 );
    assert( access( "dir/linkA/dir2", F_OK ) == 0 );
    assert( access( "dir/linkB/dir2", F_OK ) == 0 );
    assert( access( "dir/linkC/dir2", F_OK ) == 0 );

    return 0;
}
EOF

llvm_verify valid <<EOF
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
EOF

llvm_verify valid <<EOF
#include <unistd.h>
#include <sys/stat.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>

int main() {
    char buf[ 8 ] = {};

    int fd = open( "file", O_CREAT | O_WRONLY, 0644 );
    assert( fd >= 0 );
    assert( close( fd ) == 0 );

    assert( mkdir( "dir", 0755 ) == 0 );

    errno = 0;
    assert( readlink( "file", buf, 7 ) == -1 );
    assert( errno == EINVAL );

    errno = 0;
    assert( readlink( "dir", buf, 7 ) == -1 );
    assert( errno == EINVAL );

    return 0;
}
EOF
