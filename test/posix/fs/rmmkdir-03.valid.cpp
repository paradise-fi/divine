/* TAGS: ext c++ */
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
