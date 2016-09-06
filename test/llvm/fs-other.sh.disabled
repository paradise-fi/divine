. lib
. flavour vanilla

llvm_verify valid <<EOF
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

int main() {
    int fd = open( "file", O_CREAT | O_WRONLY, 0644 );
    assert( fd >= 0 );
    assert( close( fd ) == 0 );

    assert( rename( "file", "renamed" ) == 0 );

    errno = 0;
    assert( access( "file", F_OK ) == -1 );
    assert( errno == ENOENT );

    int r = access( "renamed", F_OK );
    assert( r == 0 );

    return 0;
}
EOF

llvm_verify valid <<EOF
#include <unistd.h>
#include <assert.h>
#include <string.h>

int main() {
    char input[ 10 ] =    { 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80, 0x90, 0xA0 };
    char output[ 10 ];
    char expected[ 10 ] = { 0x20, 0x10, 0x40, 0x30, 0x60, 0x50, 0x80, 0x70, 0xA0, 0x90 };

    swab( input, output, 10 );
    assert( memcmp( output, expected, 10 ) == 0 );

    return 0;
}
EOF

llvm_verify valid <<EOF
#include <unistd.h>
#include <assert.h>
#include <errno.h>

void test( int( *toTest )( int ) ) {
    assert( toTest( 0 ) == 0 );
    errno = 0;
    assert( toTest( 20 ) == -1 );
    assert( errno == EBADF );
}

int main() {
    test( fsync );
    test( fdatasync );
    test( syncfs );
}
EOF

llvm_verify valid <<EOF
#include <unistd.h>
#include <assert.h>

int main() {
    int original = umask( 123 );
    assert( umask( original | 01000 ) == 123 );
    assert( umask( original ) == original );
    return 0;
}
EOF

llvm_verify valid <<EOF
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

int main() {
    assert( mkdir( "dir", 0755 ) == 0 );
    assert( chdir( "dir" ) == 0 );
    int fd = open( "file", O_CREAT | O_WRONLY, 0644 );
    assert( fd >= 0 );
    assert( close( fd ) == 0 );

    assert( chdir( ".." ) == 0 );
    assert( access( "dir", X_OK | W_OK | R_OK ) == 0 );
    assert( access( "dir/file", W_OK | R_OK ) == 0 );

    return 0;
}
EOF

llvm_verify valid <<EOF
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

int main() {
    int fd = open( "file", O_CREAT | O_WRONLY, 0644 );
    assert( fd >= 0 );
    assert( close( fd ) == 0 );

    errno = 0;
    assert( chdir( "file" ) == -1 );
    assert( errno == ENOTDIR );

    errno = 0;
    assert( chdir( "nonexistent" ) == -1 );
    assert( errno == ENOENT );

    assert( mkdir( "dir", 0644 ) == 0 );
    errno = 0;
    assert( chdir( "dir" ) == -1 );
    assert( errno == EACCES );

    return 0;
}
EOF

llvm_verify valid <<EOF
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

int main() {
    int fd = open( "file", O_CREAT | O_WRONLY, 0644 );
    assert( fd >= 0 );

    errno = 0;
    assert( fchdir( fd ) == -1 );
    assert( errno == ENOTDIR );

    assert( close( fd ) == 0 );


    errno = 0;
    assert( fchdir( 20 ) == -1 );
    assert( errno == EBADF );

    assert( mkdir( "dir", 0644 ) == 0 );
    fd = open( "dir", O_RDONLY );
    errno = 0;
    assert( fchdir( fd ) == -1 );
    assert( errno == EACCES );

    assert( close( fd ) == 0 );

    return 0;
}
EOF
