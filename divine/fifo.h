// -*- C++ -*- (c) 2008 Petr Rockai <me@mornfall.net>

#include <wibble/sys/mutex.h>
#include <divine/blob.h> // for blobby fifos
#include <deque>

#ifndef DIVINE_FIFO_H
#define DIVINE_FIFO_H

namespace divine {
// fixme?
using namespace wibble::sys;

const int cacheLine = 64;

struct NoopMutex {
    void lock() {}
    void unlock() {}
};

/**
 * A simple queue (First-In, First-Out). Concurrent access to the ends of the
 * queue is supported -- a thread may write to the queue while another is
 * reading. Concurrent access to a single end is not supported. By default, the
 * write end is protected by a mutex (to prevent multiple writers from
 * accessing a given queue concurrently), but this can be disabled by setting
 * the WM template parameter to NoopMutex. For the NoopMutex case, care has to
 * be taken to avoid concurrent access by other means.
 *
 * The NodeSize parameter defines a size of single block of objects. By
 * default, we make the node a page-sized object -- this seems to work well in
 * practice. We rely on the allocator to align the allocated blocks reasonably
 * to give good cache usage.
 */
template< typename T, typename WM = Mutex,
          int NodeSize = (4096 - cacheLine - sizeof(int)
                          - sizeof(void*)) / sizeof(T) >
struct Fifo {
protected:
    // the Node layout puts read and write counters far apart to avoid
    // them sharing a cache line, since they are always written from
    // different threads
    struct Node {
        int read;
        char padding[ cacheLine - sizeof(int) ];
        T buffer[ NodeSize ];
        // TODO? we can save some multiplication instructions by using
        // pointers/iterators here, where we can work with addition
        // instead which is very slightly cheaper
        volatile int write;
        Node *next;
        Node() {
            read = write = 0;
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
    typedef WM WriteMutex;
    WM writeMutex;

    Fifo() {
        head = tail = new Node();
        assert( empty() );
    }

    // copying a fifo is not allowed
    Fifo( const Fifo &f ) {
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

    void push( const MutexLockT< WriteMutex > &l, const T&x ) {
        Node *t;
        assert( l.locked );
        assert( &l.mutex == &writeMutex );
        if ( tail->write == NodeSize )
            t = new Node();
        else
            t = tail;

        t->buffer[ t->write ] = x;
        ++ t->write;

        if ( tail != t ) {
            tail->next = t;
            tail = t;
        }
    }

    void push( const T& x ) {
        MutexLockT< WriteMutex > __l( writeMutex );
        push( __l, x );
    }

    bool empty() {
        return head == tail && head->read >= head->write;
    }

    void dropHead() {
        Node *old = head;
        head = head->next;
        assert( head );
        delete old;
    }

    void pop() {
        assert( !empty() );
        ++ head->read;
        if ( head->read == NodeSize ) {
            if ( head->next != 0 ) {
                dropHead();
            }
        }
        // the following can happen when head->next is 0 even though head->read
        // has reached NodeSize, *and* no front() has been called in the meantime
        if ( head->read > NodeSize ) {
            dropHead();
            pop();
        }
    }

    T &front( bool wait = false ) {
        while ( wait && empty() );
        assert( head );
        assert( !empty() );
        // last pop could have left us with empty queue exactly at an
        // edge of a block, which leaves head->read == NodeSize
        if ( head->read == NodeSize ) {
            dropHead();
        }
        return head->buffer[ head->read ];
    }
};

template< typename N >
inline void push( Fifo< Blob, NoopMutex > &fifo, const N &n ) {
    Blob b( sizeof( N ) );
    b.template get< N >() = n;
    fifo.push( b );
}

template<>
inline void push< Blob >( Fifo< Blob, NoopMutex > &fifo, const Blob &b ) {
    fifo.push( b );
}

}

#endif
