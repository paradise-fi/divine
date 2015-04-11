. lib
. flavour vanilla

llvm_verify valid <<EOF
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

void main() {
    char buf[8] = {};
    int fd = open( "test", O_WRONLY | O_CREAT, 0644 );
    assert( fd >= 0 );
    assert( write( fd, "tralala", 7 ) == 7 );
    assert( close( fd ) == 0 );
    fd = open( "test", O_RDONLY );
    assert( fd >= 0 );
    assert( read( fd, buf, 7 ) == 7 );
    assert( strcmp( "tralala", buf ) == 0 );
    assert( close( fd ) == 0 );
}
EOF

llvm_verify valid <<EOF
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>

void main() {
    char buf[8] = {};
    int fd = open( "test", O_WRONLY | O_CREAT, 0644  );
    assert( fd >= 0 );
    assert( read( fd, buf, 7 ) == -1 );
    assert( errno == EBADF );
    assert( close( fd ) == 0 );
    errno = 0;
    assert( write( fd, "x", 1 ) == -1 );
    assert( errno == EBADF );
}
EOF

llvm_verify valid <<EOF
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>

void main() {
    int fd = open( "test", O_WRONLY | O_CREAT, 0644 );
    assert( fd >= 0 );
    assert( close( fd ) == 0 );

    fd = open( "test", O_RDONLY );
    assert( write( fd, "x", 1 ) == -1 );
    assert( errno == EBADF );
    assert( close( fd ) == 0 );
}
EOF

llvm_verify valid <<EOF
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>

void main() {
    char buf[8]={};
    assert( mkdir( "dir", 0755 ) == 0 );
    int fd = open( "dir", O_RDONLY );
    assert( fd >= 0 );
    assert( read( fd, buf, 7 ) == -1 );
    assert( errno == EBADF );
    assert( close( fd ) == 0 );

}
EOF
