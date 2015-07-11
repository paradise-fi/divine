. lib
. flavour vanilla

llvm_verify valid <<EOF
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

int main() {
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
    return 0;
}
EOF

llvm_verify valid <<EOF
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>

int main() {
    char buf[8] = {};
    int fd = open( "test", O_WRONLY | O_CREAT, 0644  );
    assert( fd >= 0 );
    assert( read( fd, buf, 7 ) == -1 );
    assert( errno == EBADF );
    assert( close( fd ) == 0 );
    errno = 0;
    assert( write( fd, "x", 1 ) == -1 );
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
    int fd = open( "test", O_WRONLY | O_CREAT, 0644 );
    assert( fd >= 0 );
    assert( close( fd ) == 0 );

    fd = open( "test", O_RDONLY );
    assert( write( fd, "x", 1 ) == -1 );
    assert( errno == EBADF );
    assert( close( fd ) == 0 );
    return 0;
}
EOF

llvm_verify valid <<EOF
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>

int main() {
    char buf[8]={};
    assert( mkdir( "dir", 0755 ) == 0 );
    int fd = open( "dir", O_RDONLY );
    assert( fd >= 0 );
    assert( read( fd, buf, 7 ) == -1 );
    assert( errno == EBADF );
    assert( close( fd ) == 0 );
    return 0;
}
EOF

llvm_verify valid <<EOF
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>

int main() {
    char buf[8]={};
    int fd = open( "test", O_WRONLY | O_CREAT, 0644 );
    assert( fd >= 0 );
    assert( write( fd, "tralala", 7 ) == 7 );
    assert( close( fd ) == 0 );

    fd = open( "test", O_RDWR );
    assert( fd >= 0 );
    assert( read( fd, buf, 7 ) == 7 );
    assert( strcmp( buf, "tralala" ) == 0 );
    assert( pwrite( fd, "h", 1, 0 ) == 1 );
    assert( pwrite( fd, "b", 1, 3 ) == 1 );
    assert( pread( fd, buf, 7, 0 ) == 7 );
    assert( strcmp( buf, "hrabala" ) == 0 );
    assert( close( fd ) == 0 );
    return 0;
}
EOF

llvm_verify valid <<EOF
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <string.h>
#include <errno.h>

int main() {
    char buf[ 8 ] = {};
    int fd = open( "file.txt", O_RDWR | O_CREAT, 0644 );
    assert( fd >= 0 );
    assert( write( fd, "tralala", 7 ) == 7 );

    assert( ftruncate( fd, 4 ) == 0 );
    assert( lseek( fd, 0, SEEK_SET ) == 0 );
    assert( read( fd, buf, 7 ) == 4 );
    assert( strcmp( buf, "tral" ) == 0 );

    assert( close( fd ) == 0 );

    errno = 0;
    assert( truncate( "file.txt", -1 ) == -1 );
    assert( errno == EINVAL );
    assert( truncate( "file.txt", 0 ) == 0 );

    memset( buf, 0, 8 );
    fd = open( "file.txt", O_RDONLY );
    assert( fd >= 0 );
    assert( read( fd, buf, 7 ) == 0 );
    assert( strcmp( buf, "" ) == 0 );
    assert( close( fd ) == 0 );
    return 0;
}
EOF

llvm_verify valid <<EOF
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>

int main() {
    errno = 0;
    assert( ftruncate( 20, 0 ) == -1 );
    assert( errno == EBADF );

    assert( mkdir( "foo", 0755 ) == 0 );
    int fd = open( "foo", O_RDONLY );
    assert( fd >= 0 );
    errno = 0;
    assert( ftruncate( fd, 0 ) == -1 );
    assert( errno == EINVAL );
    assert( close( fd ) == 0 );

    errno = 0;
    assert( truncate( "foo", 0 ) == -1 );
    assert( errno == EISDIR );

    errno = 0;
    assert( truncate( "nonexistent", 0 ) == -1 );
    assert( errno == ENOENT );
    return 0;
}
EOF

llvm_verify valid <<EOF
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

int main() {
    char buf[ 8 ] = {};
    int fd = open( "file", O_CREAT | O_RDWR, 0644 );
    int dupfd;
    assert( fd >= 0 );

    assert( write( fd, "tralala", 7 ) == 7 );
    dupfd = dup( fd );
    assert( dupfd >= 0 );
    assert( dupfd != fd );

    assert( lseek( dupfd, 0, SEEK_CUR ) == 7 );
    assert( lseek( dupfd, 0, SEEK_SET ) == 0 );
    assert( lseek( fd, 0, SEEK_CUR ) == 0 );

    assert( close( fd ) == 0 );

    assert( read( dupfd, buf, 7 ) == 7 );
    assert( strcmp( buf, "tralala" ) == 0 );

    assert( close( dupfd ) == 0 );
    return 0;
}
EOF

llvm_verify valid <<EOF
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

void writeFile( const char *name, const char *content ) {
    int fd = open( name, O_CREAT | O_RDWR, 0644 );
    assert( fd >= 0 );

    int size = strlen( content );
    assert( write( fd, content, size ) == size );
    assert( close( fd ) == 0 );
}

int main() {
    char buf[ 8 ] = {};
    writeFile( "file1", "tralala" );
    writeFile( "file2", "hrabala" );

    int fd = open( "file1", O_RDONLY );
    assert( fd >= 0 );
    assert( read( fd, buf, 7 ) == 7 );
    assert( strcmp( buf, "tralala" ) == 0 );

    int newfd = open( "file2", O_RDONLY );
    assert( newfd >= 0 );

    int dupfd = dup2( newfd, fd );
    assert( dupfd == fd );

    assert( read( fd, buf, 7 ) == 7 );
    assert( strcmp( buf, "hrabala" ) == 0 );

    assert( close( fd ) == 0 );
    assert( close( newfd ) == 0 );

    return 0;
}
EOF

llvm_verify valid <<EOF
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

int main() {

    int fd = open( "file", O_CREAT | O_RDWR, 0644 );
    assert( fd >= 0 );

    assert( write( fd, "tralala", 7 ) == 7 );

    assert( lseek( fd, 0, SEEK_CUR ) == 7 );
    assert( lseek( fd, -7, SEEK_CUR ) == 0 );
    assert( lseek( fd, -5, SEEK_END ) == 2 );
    assert( lseek( fd, 5, SEEK_END ) == 7 + 5 );

    errno = 0;
    assert( lseek( fd, -1, SEEK_SET ) == -1 );
    assert( errno == EINVAL );

    errno = 0;
    assert( lseek( fd, -8, SEEK_END ) == -1 );
    assert( errno == EINVAL );

    assert( lseek( fd, 2, SEEK_SET ) == 2 );
    errno = 0;
    assert( lseek( fd, -3, SEEK_CUR ) == -1 );
    assert( errno == EINVAL );

    errno = 0;
    assert( lseek( fd, 0, 20 ) == -1 );
    assert( errno == EINVAL );

    assert( close( fd ) == 0 );
    return 0;
}
EOF
