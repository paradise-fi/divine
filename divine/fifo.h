// -*- C++ -*- (c) 2008 Petr Rockai <me@mornfall.net>

#include <divine/threading.h> // for cacheLine
#include <divine/blob.h> // for blobby fifos

#ifndef DIVINE_FIFO_H
#define DIVINE_FIFO_H

namespace divine {
// fixme?
using namespace wibble::sys;

struct NoopMutex {
    void lock() {}
    void unlock() {}
};

// make the node a page-sized object by default; hopefully, the
// allocator will figure to page-align the object which gives optimal
// cache usage
template< typename T, typename WM = Mutex,
          int NodeSize = (4096 - cacheLine - sizeof(int)
                          - sizeof(void*)) / sizeof(T) >
struct Fifo {
protected:
    WM writeMutex;
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
    Node *head, *freetail;
    char _padding2[cacheLine-2*sizeof(Node*)];
    Node *tail, *freehead;
    char _padding3[cacheLine-2*sizeof(Node*)];

public:
    Fifo() {
        head = tail = new Node();
        freehead = freetail = new Node();
        assert( empty() );
    }

    // copying a fifo is not allowed
    Fifo( const Fifo &f ) {
        head = tail = new Node();
        freehead = freetail = new Node();
        assert( empty() );
    }

    virtual ~Fifo() {} // TODO free up stuff here

    void push( const T& x ) {
        Node *t;
        writeMutex.lock();
        if ( tail->write == NodeSize ) {
            if ( freehead != freetail ) {
                // grab a node for recycling
                t = freehead;
                assert( freehead->next != 0 );
                freehead = freehead->next;

                // clear it
                t->read = t->write = 0;
                t->next = 0;

                // dump the rest of the freelist
                while ( freehead != freetail ) {
                    Node *next = freehead->next;
                    assert( next != 0 );
                    delete freehead;
                    freehead = next;
                }

                assert( freehead == freetail );
            } else {
                t = new Node();
            }
        } else
            t = tail;

        t->buffer[ t->write ] = x;
        ++ t->write;

        if ( tail != t ) {
            tail->next = t;
            tail = t;
        }
        writeMutex.unlock();
    }

    bool empty() {
        return head == tail && head->read >= head->write;
    }

    void dropHead() {
        Node *old = head;
        head = head->next;
        assert( head );
        old->next = 0;
        freetail->next = old;
        freetail = old;
    }

    void pop() {
        assert( !empty() );
        ++ head->read;
        if ( head->read == NodeSize ) {
            if ( head->next != 0 ) {
                dropHead();
            }
        }
    }

    T &front() {
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
inline void push( Fifo< Blob > &fifo, const N &n ) {
    Blob b( sizeof( N ) );
    b.template get< N >() = n;
    fifo.push( b );
}

template<>
inline void push< Blob >( Fifo< Blob > &fifo, const Blob &b ) {
    fifo.push( b );
}

}

#endif
