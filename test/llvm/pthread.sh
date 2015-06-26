. lib
. flavour vanilla

llvm_verify valid <<EOF
#include <assert.h>
#include <pthread.h>

void *thread( void *x ) {
     return 1;
}

void main() {
     pthread_t tid;
     pthread_create( &tid, NULL, thread, NULL );
     void *i = 0;
     pthread_join( tid, &i );
     assert( i == 1 );
}
EOF

llvm_verify valid <<EOF
#include <assert.h>
#include <pthread.h>

int shared = 0;
pthread_mutex_t mutex;

void *thread( void *x ) {
     pthread_mutex_lock( &mutex );
     ++ shared;
     pthread_mutex_unlock( &mutex );
}

void main() {
     pthread_t tid;
     pthread_mutex_init( &mutex, NULL );
     pthread_create( &tid, NULL, thread, NULL );
     pthread_mutex_lock( &mutex );
     ++ shared;
     pthread_mutex_unlock( &mutex );
     pthread_join( tid, NULL );
     assert( shared == 2 );
}
EOF

llvm_verify invalid assert testcase.c:15 <<EOF
#include <assert.h>
#include <pthread.h>

int shared = 0;

void *thread( void *x ) {
     ++ shared;
}

void main() {
     pthread_t tid;
     pthread_create( &tid, NULL, thread, NULL );
     ++ shared;
     pthread_join( tid, NULL );
     assert( shared == 2 );
}
EOF

llvm_verify valid <<EOF
#include <assert.h>
#include <pthread.h>

void *thread( void *x ) {
     pthread_exit( 1 );
     return 0;
}

void main() {
     pthread_t tid;
     pthread_create( &tid, NULL, thread, NULL );
     void *i = 0;
     pthread_join( tid, &i );
     assert( i == 1 );
}
EOF

llvm_verify valid <<EOF
#include <assert.h>
#include <pthread.h>

void *thread( void *x ) {
     return 1;
}

void main() {
    pthread_t detached[ 2 ];
    pthread_attr_t startDetach;
    pthread_attr_init( &startDetach );
    pthread_attr_setdetachstate( &startDetach, PTHREAD_CREATE_DETACHED );
    pthread_create( &detached[ 0 ], &startDetach, thread, NULL );
    pthread_create( &detached[ 1 ], NULL, thread, NULL );
    pthread_t tid;
    pthread_create( &tid, NULL, thread, NULL );
    pthread_detach( detached[ 1 ] );
    void *i = 0;
    pthread_join( tid, &i );
    assert( i == 1 );
    pthread_attr_destroy( &startDetach );
}
EOF

llvm_verify valid <<EOF
#include <assert.h>
#include <pthread.h>

void *thread( void *x ) {
     return 0;
}

void main() {
     pthread_t tid;
     pthread_create( &tid, NULL, thread, NULL );
     pthread_join( tid, NULL );
     int ret = 1;
     pthread_exit( &ret );
}
EOF
