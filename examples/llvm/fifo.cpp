// -*- C++ -*- (c) 2008-2011 Petr Rockai <me@mornfall.net>

/*
 * This program is a simple test case for the divine intra-thread fifo, the
 * tool's main shared-memory communication primitive. We fire off two threads,
 * a writer and a reader. The writer pushes 3 values, while the reader checks
 * that the values written match the values read, that the fifo is
 * empty/non-empty at the right places &c.
 *
 * Verify with:
 *  $ divine compile --llvm [--cflags=" < flags > "] fifo.cpp
 *  $ divine reachability fifo.bc --ignore-deadlocks [-d]
 * Execute with:
 *  $ clang [ < flags > ] -lpthread -o fifo.exe fifo.cpp
 *  $ ./fifo.exe
 */

/*
 * Future versions of DiVinE should also provide new/delete based on the builtin
 * malloc/free. When that is done, it should be possible to #include unmodified
 * fifo.h here. We substitute malloc/free manually for now.
 */

#include <pthread.h>
#include <cstdlib>

// For native execution (in future we will provide cassert).
#ifndef DIVINE
#include <cassert>
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
        T * read;
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
    Node * head;
    char _padding2[cacheLine-2*sizeof(Node*)];
    Node * volatile tail;
    char _padding3[cacheLine-2*sizeof(Node*)];

public:
    Fifo() {
        head = tail = mkNode();
        assert( empty() );
    }

    // copying a fifo is not allowed
    Fifo( const Fifo & ) {
        head = tail = mkNode();
        assert( empty() );
    }

    ~Fifo() {
        while ( head != tail ) {
            Node *next = head->next;
            assert( next != 0 );
            free(head);
            head = next;
        }
        free(head);
    }

    Node *mkNode() {
        Node *n = (Node *)malloc(sizeof (Node));
        n->read = n->write = n->buffer;
        n->next = 0;
        return n;
    }

    void push( const T&x ) {
        Node *t;
        if ( tail->write == tail->buffer + NodeSize ) {
            t = mkNode();
        } else
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
        free(old);
    }

    void pop() {
        assert( !empty() );
        ++ head->read;
        if ( head->read == head->buffer + NodeSize ) {
            if ( head->next != 0 ) {
                dropHead();
            }
        }
        // the following can happen when head->next is 0 even though head->read
        // has reached NodeSize, *and* no front() has been called in the meantime
        if ( head->read > head->buffer + NodeSize ) {
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


void * reader( void * in ) {
   Fifo< int > * volatile f = (Fifo< int >*) in;
   for ( int i = 0; i < 3; ++i ) {
         int j = f->front( true );
         assert( i == j );
         assert( !f->empty() );
         f->pop();
	 // if (i<2) assert (!f->empty() ); //should assert non-deterministically
   }
   assert( f->empty() );
   while ( true );
   return 0;
};


void * writer( void * in) {
   Fifo< int > * volatile f = (Fifo< int >*) in;
   for ( int i = 0; i < 3; ++i )
         f->push( i );
   while( true );
   return 0;
};


int main() {
    Fifo< int > f;
    void * r;
    pthread_t p1,p2;
    pthread_create(&p1,0,reader,&f);
    pthread_create(&p2,0,writer,&f);
    pthread_join(p1,&r);
    pthread_join(p2,&r);
    return 0;
}
