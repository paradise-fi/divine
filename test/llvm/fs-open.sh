. lib

llvm_verify valid <<EOF
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>

void main() {
    int fd = open( "test", O_RDONLY );
    assert( fd == -1 );
    assert( errno == ENOENT );
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

    fd = open( "test", O_WRONLY | O_CREAT | O_EXCL, 0644  );
    assert( fd == -1 );
    assert( errno == EEXIST );
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
    assert( close( fd ) == -1 );
    assert( errno == EBADF );
}
EOF

llvm_verify valid <<EOF
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>

void main() {
    assert( mkdir( "dir", 0755 ) == 0 );

    int fd = open( "dir", O_RDONLY );
    assert( fd >= 0 );
    assert( close( fd ) == 0 );

    fd = open( "dir", O_WRONLY );
    assert( fd == -1 );
    assert( errno == EISDIR );
}
EOF
