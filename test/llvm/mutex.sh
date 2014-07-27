. lib

# re-locking
llvm_verify invalid "PTHREAD DEADLOCK" "Deadlock: Nonrecursive mutex locked again" <<EOF
// re-locking
#include <pthread.h>

int main() {
    pthread_mutex_t b = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_lock( &b );
    pthread_mutex_lock( &b );

    return 0;
}
EOF

# dead locking thread
llvm_verify invalid "PTHREAD DEADLOCK" "Deadlock: Mutex locked by dead thread" <<EOF
// dead locking thread
#include <pthread.h>

void *thr( void *mtx ) {
    pthread_mutex_lock( mtx );
    return 0;
}

int main() {
    pthread_mutex_t b = PTHREAD_MUTEX_INITIALIZER;
    pthread_t t;
    pthread_create( &t, 0, thr, &b );
    pthread_mutex_lock( &b );
    pthread_join( t, 0 );

    return 0;
}
EOF

# cycle
llvm_verify invalid "PTHREAD DEADLOCK" "Deadlock: Mutex cycle closed" <<EOF
// basic cycle
#include <pthread.h>

volatile int i = 0;

void *thr( void *mtx ) {
    pthread_mutex_t *mutex1 = ( (pthread_mutex_t **)mtx )[ 0 ];
    pthread_mutex_t *mutex2 = ( (pthread_mutex_t **)mtx )[ 1 ];
    pthread_mutex_lock( mutex1 );
    pthread_mutex_lock( mutex2 );
    i = (i + 1) % 3;
    pthread_mutex_unlock( mutex2 );
    pthread_mutex_unlock( mutex1 );
    return 0;
}

int main() {
    pthread_mutex_t a = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t b = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t *ab[] = { &a, &b };
    pthread_mutex_t *ba[] = { &b, &a };

    pthread_t ta;

    pthread_create( &ta, 0, thr, &ab );
    thr( ba );
    pthread_join( ta, 0 );

    return 0;
}
EOF

# complicated cycle with partial deadlock
llvm_verify invalid "PTHREAD DEADLOCK" "Deadlock: Mutex cycle closed" <<EOF
// complicated cycle partial deadlock
#include <pthread.h>

volatile int i = 0, j = 0;

void *thr( void *mtx ) {
    pthread_mutex_t *mutex1 = ( (pthread_mutex_t **)mtx )[ 0 ];
    pthread_mutex_t *mutex2 = ( (pthread_mutex_t **)mtx )[ 1 ];
    pthread_mutex_lock( mutex1 );
    pthread_mutex_lock( mutex2 );
    i = (i + 1) % 3;
    pthread_mutex_unlock( mutex2 );
    pthread_mutex_unlock( mutex1 );
    return 0;
}

void *other( void *_ ) {
    while ( 1 )
        j = (j + 1) % 3;
}

int main() {
    pthread_mutex_t a = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t b = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t c = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t *ab[] = { &a, &b };
    pthread_mutex_t *bc[] = { &b, &c };
    pthread_mutex_t *ca[] = { &c, &a };

    pthread_mutex_t *ba[] = { &b, &a };

    pthread_t ta, tb, ot;

    pthread_create( &ot, 0, other, 0 );

    pthread_create( &ta, 0, thr, &ab );
    pthread_create( &tb, 0, thr, &bc );
    thr( ca );
    pthread_join( ta, 0 );
    pthread_join( tb, 0 );
    for ( ;; );
    return 0;
}
EOF

llvm_verify valid <<EOF
// right ordering
#include <pthread.h>

volatile int i = 0;

void *thr( void *mtx ) {
    pthread_mutex_t *mutex1 = ( (pthread_mutex_t **)mtx )[ 0 ];
    pthread_mutex_t *mutex2 = ( (pthread_mutex_t **)mtx )[ 1 ];
    pthread_mutex_lock( mutex1 );
    pthread_mutex_lock( mutex2 );
    i = (i + 1) % 3;
    pthread_mutex_unlock( mutex2 );
    pthread_mutex_unlock( mutex1 );
    return 0;
}

int main() {
    pthread_mutex_t a = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t b = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t *ab[] = { &a, &b };

    pthread_t ta;

    pthread_create( &ta, 0, thr, &ab );
    thr( ab );
    pthread_join( ta, 0 );

    return 0;
}
EOF


