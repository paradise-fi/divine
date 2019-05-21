# TAGS: ext capture
. lib/testcase

mkdir -p capture

cat > capture/fs-rmmkdir-capture.cpp <<EOF
/* VERIFY_OPTS: --capture capture/link:follow:/ */
#include <unistd.h>
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

    createDir( "dir", EEXIST ); //captured
    createDir( "dir/subdir1" );
    
    createDir( "dir/subdir1", EEXIST );
    createFile( "dir/subdir1", EEXIST );

    createDir( "file", EEXIST );//captured

    createFile( "dir", EEXIST );
    createFile( "dir/subdir2" );

    createDir( "dir/subdir2", EEXIST );
    createFile( "dir/file" );

   removeItem( rmdir, "dir", ENOTEMPTY );
   removeItem( unlink, "dir", EISDIR );
   removeItem( rmdir, "dir/subdir1" );
   removeItem( unlink, "dir/subdir2" );
   removeItem( rmdir, "dir", ENOTEMPTY );
   removeItem( unlink, "dir/file" );
   removeItem( rmdir, "dir" ); //should be empty -> check test/posix/files/link

    return 0;
}
EOF

mkdir capture/link
mkdir capture/link/dir
touch capture/link/file
ln capture/link/file capture/link/hardlinkFile

verify capture/fs-rmmkdir-capture.cpp
