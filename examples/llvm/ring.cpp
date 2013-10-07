/*
 * Name
 * ====================
 *  Ring
 *
 * Category
 * ====================
 *  Data structures
 *
 * Short description
 * ====================
 *  Lock-free inter-thread ring buffer.
 *
 * Long description
 * ====================
 *  This program is an example of a lock-free inter-thread ring buffer.
 *  Verification should proceed without detecting any safety violation.
 *
 * Verification
 * ====================
 *     $ divine compile --llvm [--cflags=" < flags > "] ring.cpp
 *     $ divine verify -p assert ring.bc [-d]
 *
 * Execution
 * ====================
 *     $ clang++ [ < flags > ] -lpthread -o ring.exe ring.cpp
 *     $ ./ring.exe
 *
 * Standard
 * ====================
 *  C++98
 */

#include <pthread.h>
#include <stdlib.h>
#include <assert.h>

template< typename T, int size >
struct Ring {
    volatile int reader;
    T q[ size ];
    volatile int writer;

    void enqueue( T x ) {
        while ( (writer + 1) % size == reader ); // full; need to wait
        q[ writer ] = x;
        writer = (writer + 1) % size;
    }

    T dequeue() {
        T x = q[ reader ];
        reader = (reader + 1) % size;
        return x;
    }

    bool empty() {
        return reader == writer;
    }

    Ring() : reader( 0 ), writer( 0 ) {}
};

typedef Ring< int, 8 > R;

void * reader( void * in ) {
    R * volatile r = (R *) in;
    for ( int i = 0; i < 10; ++i ) {
        while ( r->empty() );
        int j = r->dequeue();
        assert( i == j );
    }

    assert( r->empty() );
    return 0;
}

void * writer( void * in) {
    R * volatile r = (R *) in;
    for ( int i = 0; i < 10; ++i )
        r->enqueue( i );
    return 0;
}

int main() {
    R r;
    pthread_t p1, p2;
    pthread_create(&p1, NULL, reader, &r);
    pthread_create(&p2, NULL, writer, &r);
    pthread_join(p1, NULL);
    pthread_join(p2, NULL);
    return 0;
}
