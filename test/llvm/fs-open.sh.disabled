. lib
. flavour vanilla

llvm_verify valid <<EOF
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>

int main() {
    int fd = open( "test", O_RDONLY );
    assert( fd == -1 );
    assert( errno == ENOENT );
    return 0;
}
EOF

llvm_verify valid <<EOF
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>

int main() {
    int fd = open( "test", O_WRONLY | O_CREAT, 0644 );
    assert( fd >= 0 );
    assert( close( fd ) == 0 );

    fd = open( "test", O_WRONLY | O_CREAT | O_EXCL, 0644  );
    assert( fd == -1 );
    assert( errno == EEXIST );
    return 0;
}
EOF

llvm_verify valid <<EOF
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>

int main() {
    int fd = open( "test", O_WRONLY | O_CREAT, 0644 );
    assert( fd >= 0 );
    assert( close( fd ) == 0 );
    assert( close( fd ) == -1 );
    assert( errno == EBADF );
    return 0;
}
EOF

llvm_verify valid <<EOF
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>

int main() {
    assert( mkdir( "dir", 0755 ) == 0 );

    int fd = open( "dir", O_RDONLY );
    assert( fd >= 0 );
    assert( close( fd ) == 0 );

    fd = open( "dir", O_WRONLY );
    assert( fd == -1 );
    assert( errno == EISDIR );
    return 0;
}
EOF
