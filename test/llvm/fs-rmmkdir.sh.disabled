. lib
. flavour vanilla

llvm_verify valid <<EOF
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

char _n1[ 256 ];
char _n2[ 511 ];
char _n3[ 766 ];
char _n4[ 1021 ];
char _n5[ 1030 ];

char *name[] = {
    _n1, _n2, _n3, _n4, _n5
};

int main() {

    memset( name[ 0 ], 'a', 255 );
    memset( name[ 1 ], 'a', 510 );
    memset( name[ 2 ], 'a', 765 );
    memset( name[ 3 ], 'a', 1020 );

    name[ 0 ][ 255 ] = '\0';
    name[ 1 ][ 510 ] = '\0';
    name[ 2 ][ 765 ] = '\0';
    name[ 3 ][ 1020 ] = '\0';

    name[ 1 ][ 255 ] = '/';
    name[ 2 ][ 255 ] = '/';
    name[ 2 ][ 510 ] = '/';
    name[ 3 ][ 255 ] = '/';
    name[ 3 ][ 510 ] = '/';
    name[ 3 ][ 765 ] = '/';

    memcpy( name[ 4 ], name[ 3 ], 1021 );
    strcat( name[ 4 ] + 1020, "/aaaaaaa" );

    assert( mkdir( name[0], 0755 ) == 0 );
    errno = 0;
    assert( mkdir( name[0], 0755 ) == -1 );
    assert( errno == EEXIST );

    name[ 1 ][ 255 ] = 'a';
    errno = 0;
    assert( mkdir( name[ 1 ], 0755 ) == -1 );
    assert( errno == ENAMETOOLONG );
    name[ 1 ][ 255 ] = '/';

    for ( int i = 1; i < 4; ++i )
        assert( mkdir( name[ i ], 0755 ) == 0 );

    assert( access( name[ 3 ], F_OK ) == 0 );
    errno = 0;
    assert( mkdir( name[ 4 ], 0755 ) == -1 );
    assert( errno == ENAMETOOLONG );
    return 0;
}
EOF

llvm_verify valid <<EOF
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>

int main() {
    int fd = open( "file", O_WRONLY | O_CREAT, 0644 );
    assert( fd >= 0 );
    assert( close( fd ) == 0 );

    errno = 0;
    assert( mkdir( "file/dir", 0755 ) == -1 );
    int e = errno;
    assert( e == ENOTDIR );

    return 0;
}
EOF

llvm_verify_cpp valid <<EOF
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>

void createFile( const char *name, int ecode = 0 ) {
    errno = 0;
    int fd = open( name, O_WRONLY | O_CREAT | O_EXCL, 0644 );

    if ( ecode ) {
        assert( fd == -1 );
        assert( errno == ecode );
    }
    else {
        assert( fd >= 0 );
        assert( close( fd ) == 0 );
        assert( access( name, F_OK ) == 0 );
        assert( access( name, R_OK ) == 0 );
        assert( access( name, W_OK ) == 0 );
        errno = 0;
        assert( access( name, X_OK ) == -1 );
        assert( errno == EACCES );
    }
}

void createDir( const char *name, int ecode = 0 ) {
    errno = 0;
    int r = mkdir( name, 0755 );
    if ( ecode ) {
        assert( r == -1 );
        assert( errno == ecode );
    }
    else {
        assert( r == 0 );
        assert( access( name, F_OK ) == 0 );
    }
}

void removeItem( int( *f )( const char * ), const char *name, int ecode = 0 ) {
    errno = 0;
    int r = f( name );

    if ( ecode ) {
        assert( r == -1 );
        assert( errno == ecode );
    }
    else {
        assert( r == 0 );
        assert( access( name, F_OK ) == -1 );
        assert( errno == ENOENT );
    }
}

int main() {
    createDir( "dir" );
    createDir( "dir/subdir1" );
    createDir( "dir/subdir2" );
    createFile( "dir/subdir1/file" );
    createFile( "dir/file1" );
    createFile( "dir/file2" );

    createDir( "dir", EEXIST );
    createDir( "dir/subdir1", EEXIST );
    createDir( "dir/subdir2", EEXIST );
    createDir( "dir/subdir1/file", EEXIST );
    createDir( "dir/file1", EEXIST );
    createDir( "dir/file2", EEXIST );

    createFile( "dir", EEXIST );
    createFile( "dir/subdir1", EEXIST );
    createFile( "dir/subdir2", EEXIST );
    createFile( "dir/subdir1/file", EEXIST );
    createFile( "dir/file1", EEXIST );
    createFile( "dir/file2", EEXIST );

    removeItem( rmdir, "dir", ENOTEMPTY );
    removeItem( unlink, "dir", EISDIR );
    removeItem( rmdir, "dir/subdir1", ENOTEMPTY );
    removeItem( unlink, "dir/subdir1", EISDIR );
    removeItem( rmdir, "dir/subdir1/file", ENOTDIR );
    removeItem( unlink, "dir/subdir1/file" );
    removeItem( unlink, "dir/subdir1/file", ENOENT );
    removeItem( rmdir, "dir/subdir1" );
    removeItem( rmdir, "dir/subdir2" );
    removeItem( rmdir, "dir", ENOTEMPTY );
    removeItem( unlink, "dir/file1" );
    removeItem( unlink, "dir/file2" );
    removeItem( rmdir, "dir" );

    return 0;
}
EOF
