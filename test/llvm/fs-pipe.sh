. lib
. flavour vanilla

llvm_verify valid <<EOF
#include <pthread.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>

int main() {
    assert( mkfifo( "pipe", 0644 ) == 0 );

    int fd = open( "pipe", O_RDONLY );
    assert( 0 );
    return 0;
}
EOF

llvm_verify valid <<EOF
#include <pthread.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>

int main() {
    assert( mkfifo( "pipe", 0644 ) == 0 );

    int fd = open( "pipe", O_WRONLY );
    assert( 0 );
    return 0;
}
EOF

llvm_verify valid <<EOF
#include <pthread.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>

int writer;
int reader;

void *worker( void *_ ) {
    (void)_;
    char buf[ 6 ] = {};
    assert( read( reader, buf, 5 ) == 5 );
    assert( strcmp( buf, "12345" ) == 0 );

    for ( int i = 0; i < 5; ++i )
        assert( read( reader, buf + i, 1 ) == 1 );
    assert( strcmp( buf, "67890" ) == 0 );
    return NULL;
}
int main() {
    pthread_t thread;
    int pfd[ 2 ];

    assert( pipe( pfd ) == 0 );
    reader = pfd[ 0 ];
    writer = pfd[ 1 ];

    pthread_create( &thread, NULL, worker, NULL );

    assert( write( writer, "1234567", 7 ) == 7 );
    assert( write( writer, "890", 3 ) == 3 );

    pthread_join( thread, NULL );

    assert( close( reader ) == 0 );
    assert( close( writer ) == 0 );
    return 0;
}
EOF

llvm_verify valid <<EOF
#include <unistd.h>
#include <assert.h>
#include <string.h>

int main() {
    char buf[ 8 ] = {};
    int pfd[ 2 ];
    assert( pipe( pfd ) == 0 );
    read( pfd[ 0 ], buf, 7 );
    assert( 0 );
    return 0;
}
EOF

llvm_verify valid <<EOF
#include <unistd.h>
#include <assert.h>
#include <string.h>

int main() {
    char buf[ 1100 ] = {};
    int pfd[ 2 ];
    assert( pipe( pfd ) == 0 );

    assert( write( pfd[ 1 ], buf, 1100 ) == 1024 );
    write( pfd[ 1 ], buf, 1 );
    assert( 0 );
    return 0;
}
EOF

llvm_verify valid <<EOF
#include <pthread.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>

const char *message = "1234567";

void *worker( void *_ ) {
    (void)_;
    int fd = open( "pipe", O_WRONLY );
    assert( fd >= 0 );

    assert( write( fd, message, 7 ) == 7 );
    assert( write( fd, "-", 1 ) == 1 );

    assert( close( fd ) == 0 );
}

int main() {
    char buf[ 8 ] = {};
    assert( mkfifo( "pipe", 0644 ) == 0 );

    pthread_t thread;
    pthread_create( &thread, NULL, worker, NULL );

    int fd = open( "pipe", O_RDONLY );
    assert( fd >= 0 );
    char incoming;
    for ( int i = 0; ; ++i ) {
        assert( read( fd, &incoming, 1 ) == 1 );
        if ( incoming == '-' )
            break;
        buf[ i ] = incoming;
    }

    pthread_join( thread, NULL );

    assert( strcmp( buf, message ) == 0 );

    return 0;
}
EOF
