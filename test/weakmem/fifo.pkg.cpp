/* TAGS: cpp big fifo */
/* VERIFY_OPTS: -o nofail:malloc */
#include <pthread.h>
#include <stdlib.h>
#include <assert.h>
#include <atomic>

#ifdef __divine__
#include <dios.h>

// V: SC
// V: TSO:4 V_OPT: --relaxed-memory tso:4     TAGS: tso
// V: TSO:8 V_OPT: --relaxed-memory tso:8     TAGS: tso
// V: TSO:16 V_OPT: --relaxed-memory tso:16   TAGS: tso
// V: TSO:32 V_OPT: --relaxed-memory tso:32   TAGS: tso
// V: TSO:64 V_OPT: --relaxed-memory tso:64   TAGS: tso
// V: TSO:128 V_OPT: --relaxed-memory tso:128 TAGS: tso

__attribute__((constructor)) static void disbaleMallocFail() {
    __dios_configure_fault( _DiOS_SF_Malloc, _DiOS_FC_NoFail );
}
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
const int L = 3;
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
    delete q;
    return 0;
}
