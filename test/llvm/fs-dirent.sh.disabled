. lib
. flavour vanilla

llvm_verify valid <<EOF
#include <assert.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

int main() {
    assert( mkdir( "dir", 0755 ) == 0 );

    int fDot = 0;
    int f2Dots = 0;
    int fDir = 0;

    DIR *d = opendir( "" );
    assert( d );

    struct dirent *item;

    while ( item = readdir( d ) ) {
        if ( strcmp( item->d_name, "." ) == 0 ) {
            assert( !fDot );
            fDot = 1;
        }
        else if ( strcmp( item->d_name, ".." ) == 0 ) {
            assert( !f2Dots );
            f2Dots = 1;
        }
        else if ( strcmp( item->d_name, "dir" ) == 0 ) {
            assert( !fDir );
            fDir = 1;
        }
        else {
            assert( 0 );
        }
    }

    assert( fDot );
    assert( f2Dots );
    assert( fDir );

    assert( closedir( d ) == 0 );

    return 0;
}
EOF

llvm_verify valid <<EOF
#include <assert.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

int main() {
    int fd = open( "file", O_CREAT | O_WRONLY, 0644 );
    assert( fd >= 0 );
    assert( close( fd ) == 0 );

    errno = 0;
    DIR *d = opendir( "file" );
    assert( d == NULL );
    assert( errno == ENOTDIR );
    return 0;
}
EOF

llvm_verify valid <<EOF
#include <assert.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>

int main() {
    assert( mkdir( "dir", 0644 ) == 0 );
    errno = 0;
    DIR *d = opendir( "dir" );
    assert( d == NULL );
    assert( errno == EACCES );
    return 0;
}
EOF

llvm_verify valid <<EOF
#include <assert.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>

int main() {
    errno = 0;
    DIR *d = opendir( "dir" );
    assert( d == NULL );
    assert( errno == ENOENT );
    return 0;
}
EOF

llvm_verify valid <<EOF
#include <assert.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>

int main() {
    errno = 0;
    DIR *d = fdopendir( 20 );
    assert( d == NULL );
    assert( errno == EBADF );
    return 0;
}
EOF

llvm_verify valid <<EOF
#include <assert.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>

int main() {
    errno = 0;
    DIR *d = fdopendir( 1 );
    assert( d == NULL );
    assert( errno == ENOTDIR );
    return 0;
}
EOF

llvm_verify valid <<EOF
#include <assert.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>

int main() {
    errno = 0;
    struct dirent *item = readdir( NULL );
    assert( item == NULL );
    assert( errno == EBADF );
    return 0;
}
EOF

llvm_verify valid <<EOF
#include <assert.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>

int main() {
    errno = 0;
    assert( closedir( NULL ) == -1 );
    assert( errno == EBADF );
    return 0;
}
EOF

llvm_verify valid <<EOF
#include <pthread.h>
#include <assert.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>

void *worker( void *_ ) {
    (void)_;
    int fDot = 0;
    int f2Dots = 0;
    int fDir = 0;

    DIR *d = opendir( "" );
    assert( d );

    struct dirent entry;
    struct dirent *result;

    while ( 1 ) {
        assert( readdir_r( d, &entry, &result ) == 0 );
        if ( result == NULL )
            break;

        if ( strcmp( entry.d_name, "." ) == 0 ) {
            assert( !fDot );
            fDot = 1;
        }
        else if ( strcmp( entry.d_name, ".." ) == 0 ) {
            assert( !f2Dots );
            f2Dots = 1;
        }
        else if ( strcmp( entry.d_name, "dir" ) == 0 ) {
            assert( !fDir );
            fDir = 1;
        }
        else {
            assert( 0 );
        }
    }

    assert( fDot );
    assert( f2Dots );
    assert( fDir );

    assert( closedir( d ) == 0 );
    return NULL;
}

int main() {
    assert( mkdir( "dir", 0755 ) == 0 );
    pthread_t thread;
    pthread_create( &thread, NULL, worker, NULL );
    worker( NULL );
    pthread_join( thread, NULL );
    return 0;
}
EOF

llvm_verify valid <<EOF
#include <assert.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

void check( DIR *dd ) {
    int fDot = 0;
    int f2Dots = 0;
    int fDir = 0;

    struct dirent *item;

    while ( item = readdir( dd ) ) {
        if ( strcmp( item->d_name, "." ) == 0 ) {
            assert( !fDot );
            fDot = 1;
        }
        else if ( strcmp( item->d_name, ".." ) == 0 ) {
            assert( !f2Dots );
            f2Dots = 1;
        }
        else if ( strcmp( item->d_name, "dir" ) == 0 ) {
            assert( !fDir );
            fDir = 1;
        }
        else {
            assert( 0 );
        }
    }

    assert( fDot );
    assert( f2Dots );
    assert( fDir );
}

int main() {
    assert( mkdir( "dir", 0755 ) == 0 );

    DIR *dd = opendir( "" );
    assert( dd );

    long position = telldir( dd );
    check( dd );
    rewinddir( dd );
    check( dd );
    seekdir( dd, position );
    check( dd );

    assert( closedir( dd ) == 0 );

    return 0;
}
EOF

llvm_verify valid <<EOF
#include <assert.h>
#include <string.h>
#include <dirent.h>
#include <stdlib.h>
#include <sys/stat.h>

const char *names[] = {
    ".", "..", "dir"
};

void check( const char *path, int level ) {
    struct dirent **namelist;
    int n = scandir( path, &namelist, NULL, alphasort );
    assert( n == level );

    for ( int i = 0; i < n; ++i ) {
        assert( strcmp( names[ i ], namelist[ i ]->d_name ) == 0 );
        free( namelist[ i ] );
    }
    free( namelist );
}

int main() {
    assert( mkdir( "dir", 0755 ) == 0 );

    check( "", 3 );
    check( "dir", 2 );
    assert( chdir( "dir" ) == 0 );
    check( "", 2 );

    return 0;
}
EOF