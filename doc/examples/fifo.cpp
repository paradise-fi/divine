/* divine-cflags: -std=c++11 */
/*
 * Fifo
 * ====
 *
 *  Intra-thread fifo, used for shared-memory communication between threads
 *  in DiVinE.
 *
 *  *tags*: data structures, C++11
 *
 * Description
 * -----------
 *
 *  This program is a simple test case for the divine intra-thread fifo, the
 *  tool's main shared-memory communication primitive. We fire off two threads,
 *  a writer and a reader. The writer pushes 3 values, while the reader checks
 *  that the values written match the values read, that the fifo is
 *  empty/non-empty at the right places &c.
 *
 * Verification
 * ------------
 *
 *       $ divine compile --llvm fifo.cpp
 *       $ divine verify -p assert fifo.bc -d
 *       $ divine safety -p assert fifo.bc -d
 *
 * Execution
 * ---------
 *
 *       $ clang++ -lpthread -o fifo.exe fifo.cpp -std=c++11
 *       $ ./fifo.exe
 */

// -*- C++ -*- (c) 2008-2011 Petr Rockai <me@mornfall.net>

/*
 * Note: Future versions of DiVinE should also provide new/delete based on
 * the builtin malloc/free. When that is done, it should be possible to #include
 * unmodified fifo.h here. We substitute malloc/free manually for now.
 */

#include <pthread.h>
#include <stdlib.h>
#include <assert.h>
#include <atomic>

#ifdef __divine__
#include <divine.h>
#include <new>

void* operator new  ( std::size_t count ) { return __vm_obj_make( count ); }
#endif

const int cacheLine = 64;

/**
 * A simple queue (First-In, First-Out). Concurrent access to the ends of the
 * queue is supported -- a thread may write to the queue while another is
 * reading. Concurrent access to a single end is, however, not supported.
 *
 * The NodeSize parameter defines a size of single block of objects. By
 * default, we make the node a page-sized object -- this seems to work well in
 * practice. We rely on the allocator to align the allocated blocks reasonably
 * to give good cache usage.
 */
template< typename T,
          int NodeSize = (512 - cacheLine - sizeof(int)
                          - sizeof(void*)) / sizeof(T) >
struct Fifo {
protected:
    // the Node layout puts read and write counters far apart to avoid
    // them sharing a cache line, since they are always written from
    // different threads
    struct Node {
        T *read;
        char padding[ cacheLine - sizeof(int) ];
        T buffer[ NodeSize ];
        T * volatile write;
        Node *next;
        Node() {
            read = write = buffer;
            next = 0;
        }
    };

    // pad the fifo object to ensure that head/tail pointers never
    // share a cache line with anyone else
    char _padding1[cacheLine-2*sizeof(Node*)];
    Node *head;
    char _padding2[cacheLine-2*sizeof(Node*)];
    Node * volatile tail;
    char _padding3[cacheLine-2*sizeof(Node*)];

public:
    Fifo() {
        head = tail = new Node();
        assert( empty() );
    }

    // copying a fifo is not allowed
    Fifo( const Fifo & ) {
        head = tail = new Node();
        assert( empty() );
    }

    ~Fifo() {
        while ( head != tail ) {
            Node *next = head->next;
            assert( next != 0 );
            delete head;
            head = next;
        }
        delete head;
    }

    void push( const T&x ) {
        Node *t;
        if ( tail->write == tail->buffer + NodeSize )
            t = new Node();
        else
            t = tail;

        *t->write = x;
        ++ t->write;

        if ( tail != t ) {
            tail->next = t;
            tail = t;
        }
    }

    bool empty() {
        return head == tail && head->read >= head->write;
    }

    int size() {
        int size = 0;
        Node *n = head;
        do {
            size += n->write - n->read;
            n = n->next;
        } while (n);
        return size;
    }

    void dropHead() {
        Node *old = head;
        head = head->next;
        assert( !!head );
        delete old;
    }

    void pop() {
        assert( !empty() );
        ++ head->read;
        if ( head->read == head->buffer + NodeSize ) {
            if ( head != tail ) {
                dropHead();
            }
        }
        // the following can happen when head->next is 0 even though head->read
        // has reached NodeSize, *and* no front() has been called in the meantime
        if ( head != tail && head->read > head->buffer + NodeSize ) {
            dropHead();
            pop();
        }
    }

    T &front( bool wait = false ) {
        while ( wait && empty() ) ;
        assert( !!head );
        assert( !empty() );
        // last pop could have left us with empty queue exactly at an
        // edge of a block, which leaves head->read == NodeSize
        if ( head->read == head->buffer + NodeSize ) {
            dropHead();
        }
        return *head->read;
    }
};


///////////////////////////////

const int B = 2;
const int L = 2;
const int N = 2;

using Q = Fifo< int, B >;
Q *q = nullptr;
std::atomic< int > length( 0 );

void *push( void * ) {
    int i = 0;
    try {
        while ( true )
            if ( length < L ) {
                length.fetch_add( 1, std::memory_order_relaxed );
                q->push( 42 + i );
                i = (i + 1) % (N - 1);
            }
    } catch (...) {}
    return nullptr;
};

int main() {
    try {
        q = new Q;
        pthread_t p;
        pthread_create( &p, nullptr, &push, nullptr );
        while (true)
            for ( int i = 0; i < N - 1; ++i ) {
                int got = q->front( true );
                assert( 42 + i == got );
                assert( 1 <= length.load( std::memory_order_relaxed ) );
                q->pop();
                length.fetch_sub( std::memory_order_relaxed );
            }
        pthread_join( p, nullptr );
    } catch (...) {} // ignore exceptions
    delete q;
    return 0;
}

/* divine-test
holds: true
*/
/* divine-test
lart: weakmem:tso:3
holds: true
*/
/* divine-test
lart: weakmem:std:3
holds: false
problem: ASSERTION.*fifo.cpp
*/
