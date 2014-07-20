// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * Utilities and data structures for shared-memory parallelism. Includes:
 * - shared memory, lock-free first-in/first-out queue (one reader + one writer)
 * - a spinlock
 * - approximate counter (share a counter between threads without contention)
 * - a weakened atomic type (like std::atomic)
 * - a derivable wrapper around std::thread
 */

/*
 * (c) 2008, 2012 Petr Ročkai <me@mornfall.net>
 * (c) 2011 Tomáš Janoušek <tomi@nomi.cz>
 * (c) 2014 Vladimír Štill <xstill@fi.muni.cz>
 */

/* Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE. */

#include <brick-unittest.h>

#if __cplusplus >= 201103L
#include <mutex>
#include <atomic>
#include <thread>
#endif

#ifndef BRICK_SHMEM_H
#define BRICK_SHMEM_H

#ifndef BRICKS_CACHELINE
#define BRICKS_CACHELINE 64
#endif

namespace brick {
namespace shmem {

#if __cplusplus >= 201103L

struct Thread {
    std::thread *_thread;
    virtual void main() = 0;

    Thread() : _thread( nullptr ) {}
    ~Thread() { delete _thread; }

#ifdef __divine__
    void start() __attribute__((noinline)) {
        __divine_interrupt_mask();
#else
    void start() {
#endif
        _thread = new std::thread( [this]() { this->main(); } );
    }

    void join() {
        if ( _thread )
            _thread->join();
    }

    void detach() {
        if ( _thread )
            _thread->detach();
    }
};

/**
 * A spinlock implementation.
 *
 * One has to wonder why this is missing from the C++0x stdlib.
 */
struct SpinLock {
    std::atomic_flag b;

    SpinLock() : b( ATOMIC_FLAG_INIT ) {}

    void lock() {
        while( b.test_and_set() );
    }

    void unlock() {
        b.clear();
    }

    SpinLock( const SpinLock & ) = delete;
    SpinLock &operator=( const SpinLock & ) = delete;
};

/**
 * Termination detection implemented as a shared counter of open (not yet
 * processed) states. This appears to be fast because the shared counter is
 * modified very rarely -- its incremented in large steps for thousands of
 * states in advance and then adjusted down to its actual value only if the
 * queue gets empty.
 *
 * Shared counter - Σ local of all threads = actual open count.
 * local ≥ 0, hence shared is an overapproximation of the actual open count.
 * This implies partial correctness.
 *
 * Termination follows from proper calls to sync().
 */
struct ApproximateCounter {
    enum { step = 100000 };

    struct Shared {
        std::atomic< intptr_t > counter;
        Shared() : counter( 0 ) {}

        Shared( const Shared& ) = delete;
    };

    Shared &shared;
    unsigned local;

    ApproximateCounter( Shared &s ) : shared( s ), local( 0 ) {}
    ~ApproximateCounter() { sync(); }

    void sync() {
        intptr_t value = shared.counter;

        while ( local > 0 ) {
            if ( value >= local ) {
                if ( shared.counter.compare_exchange_weak( value, value - local ) )
                    local = 0;
            } else {
                if ( shared.counter.compare_exchange_weak( value, 0 ) )
                    local = 0;
            }
        }
    }

    ApproximateCounter& operator++() {
        if ( local == 0 ) {
            shared.counter += step;
            local = step;
        }

        --local;

        return *this;
    }

    ApproximateCounter &operator--() {
        ++local;
        return *this;
    }

    // NB. sync() must be called manually as this method is called too often
    bool isZero() {
        return shared.counter == 0;
    }

    void reset() { shared.counter = 0; }

    ApproximateCounter( const ApproximateCounter &a )
        : shared( a.shared ), local( a.local )
    {}
    ApproximateCounter operator=( const ApproximateCounter & ) = delete;
};

struct StartDetector {

    struct Shared {
        std::atomic< unsigned short > counter;

        Shared() : counter( 0 ) {}
        Shared( Shared & ) = delete;
    };

    Shared &shared;

    StartDetector( Shared &s ) : shared( s ) {}
    StartDetector( const StartDetector &s ) : shared( s.shared ) {}

    void waitForAll( unsigned short peers ) {
        if ( ++shared.counter == peers )
            shared.counter = 0;

        while ( shared.counter );
    }

};

/*
 * Simple wrapper around atomic with weakened memory orders.
 *
 * The WeakAtomic users consume memory order for reading and release MO for
 * writing, which should assure atomicity and consistency of given variable,
 * however it does not assure consistence of other variables written before
 * given atomic location.  Read-modify-write operations use
 * memory_order_acq_rel.
 */

namespace _impl {

template< typename Self, typename T >
struct WeakAtomicIntegral {

    T operator |=( T val ) {
        return self()._data.fetch_or( val, std::memory_order_acq_rel ) | val;
    }

    T operator &=( T val ) {
        return self()._data.fetch_and( val, std::memory_order_acq_rel ) & val;
    }

    Self &self() { return *static_cast< Self * >( this ); }
};

struct Empty { };

}

template< typename T >
struct WeakAtomic : std::conditional< std::is_integral< T >::value && !std::is_same< T, bool >::value,
                      _impl::WeakAtomicIntegral< WeakAtomic< T >, T >,
                      _impl::Empty >::type
{
    WeakAtomic( T x ) : _data( x ) { }
    WeakAtomic() = default;

    operator T() const { return _data.load( std::memory_order_consume ); }
    T operator=( T val ) {
        _data.store( val, std::memory_order_release );
        return val;
    }

  private:
    std::atomic< T > _data;
    friend struct _impl::WeakAtomicIntegral< WeakAtomic< T >, T >;
};

#endif

/*
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
          int NodeSize = (32 * 4096 - BRICKS_CACHELINE - sizeof(int)
                          - sizeof(void*)) / sizeof(T) >
struct Fifo {
protected:
    // the Node layout puts read and write counters far apart to avoid
    // them sharing a cache line, since they are always written from
    // different threads
    struct Node {
        T *read;
        char padding[ BRICKS_CACHELINE - sizeof(int) ];
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
    char _padding1[BRICKS_CACHELINE - 2*sizeof(Node*)];
    Node *head;
    char _padding2[BRICKS_CACHELINE - 2*sizeof(Node*)];
    Node * volatile tail;
    char _padding3[BRICKS_CACHELINE - 2*sizeof(Node*)];

public:
    Fifo() {
        head = tail = new Node();
        ASSERT( empty() );
    }

    // copying a fifo is not allowed
    Fifo( const Fifo & ) {
        head = tail = new Node();
        ASSERT( empty() );
    }

    ~Fifo() {
        while ( head != tail ) {
            Node *next = head->next;
            ASSERT( next != 0 );
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
        __sync_synchronize();

        if ( tail != t ) {
            tail->next = t;
            __sync_synchronize();
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
        ASSERT( head );
        delete old;
    }

    void pop() {
        ASSERT( !empty() );
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
        ASSERT( head );
        ASSERT( !empty() );
        // last pop could have left us with empty queue exactly at an
        // edge of a block, which leaves head->read == NodeSize
        if ( head->read == head->buffer + NodeSize ) {
            dropHead();
        }
        return *head->read;
    }
};

}
}

namespace brick_test {
namespace shmem {

using namespace ::brick::shmem;

#if __cplusplus >= 201103L

struct FifoTest {
    template< typename T >
    struct Checker : Thread
    {
        Fifo< T > fifo;
        int terminate;
        int n;

        void main()
        {
            std::vector< int > x;
            x.resize( n );
            for ( int i = 0; i < n; ++i )
                x[ i ] = 0;

            while (true) {
                while ( !fifo.empty() ) {
                    int i = fifo.front();
                    ASSERT_EQ( x[i % n], i / n );
                    ++ x[ i % n ];
                    fifo.pop();
                }
                if ( terminate > 1 )
                    break;
                if ( terminate )
                    ++terminate;
            }
            terminate = 0;
            for ( int i = 0; i < n; ++i )
                ASSERT_EQ( x[ i ], 128*1024 );
        }

        Checker( int _n = 1 ) : terminate( 0 ), n( _n ) {}
    };

    TEST(stress) {
        Checker< int > c;
        for ( int j = 0; j < 5; ++j ) {
            c.start();
            for( int i = 0; i < 128 * 1024; ++i )
                c.fifo.push( i );
            c.terminate = true;
            c.join();
        }
    }
};

struct StartEnd {
    static const int peers = 12;

    struct DetectorWorker : Thread {

        StartDetector detector;

        DetectorWorker( StartDetector::Shared &sh ) :
            detector( sh )
        {}

        void main() {
            detector.waitForAll( peers );
        }
    };

    TEST(startDetector) {// this test must finish
        StartDetector::Shared sh;
        std::vector< DetectorWorker > threads{ peers, DetectorWorker{ sh } };

#ifdef POSIX // hm
        alarm( 1 );
#endif

        for ( int i = 0; i != 4; ++i ) {
            for ( auto &w : threads )
                w.start();
            for ( auto &w : threads )
                w.join();
            ASSERT_EQ( sh.counter.load(), 0 );
        }
    }

    struct CounterWorker : Thread {
        StartDetector detector;
        ApproximateCounter counter;
        std::atomic< int > &queue;
        std::atomic< bool > &interrupted;
        int produce;
        int consume;

        template< typename D, typename C >
        CounterWorker( D &d, C &c, std::atomic< int > &q, std::atomic< bool > &i ) :
            detector( d ),
            counter( c ),
            queue( q ),
            interrupted( i ),
            produce( 0 ),
            consume( 0 )
        {}

        void main() {
            detector.waitForAll( peers );
            while ( !counter.isZero() ) {
                ASSERT_LEQ( 0, queue.load() );
                if ( queue == 0 ) {
                    counter.sync();
                    continue;
                }
                if ( produce ) {
                    --produce;
                    ++queue;
                    ++counter;
                }
                if ( consume ) {
                    int v = queue;
                    if ( v == 0 || !queue.compare_exchange_strong( v, v - 1 ) )
                        continue;
                    --consume;
                    --counter;
                }
                if ( interrupted )
                    break;
            }
        }
    };

    void process( bool terminateEarly ) {
        StartDetector::Shared detectorShared;
        ApproximateCounter::Shared counterShared;
        std::atomic< bool > interrupted( false );

        // queueInitials
        std::atomic< int > queue{ 1 };
        ApproximateCounter c( counterShared );
        ++c;
        c.sync();

        std::vector< CounterWorker > threads{ peers,
            CounterWorker{ detectorShared, counterShared, queue, interrupted } };

#ifdef __unix
        alarm( 5 );
#endif

        // set consume and produce limits to each worker
        int i = 1;
        for ( auto &w : threads ) {
            w.produce = i;
            // let last worker consume the rest of produced values
            w.consume = peers - i;
            if ( w.consume == 0 )
                w.consume = peers + 1;// also initials
            ++i;
        }

        for ( auto &w : threads )
            w.start();

        if ( terminateEarly ) {
            interrupted = true;
            counterShared.counter = 0;
            queue = 0;
        }

        for ( auto &w : threads )
            w.join();

        if ( !terminateEarly ) { // do not check on early termination
            ASSERT_EQ( queue.load(), 0 );
            ASSERT_EQ( counterShared.counter.load(), 0 );
        }
    }

    TEST(approximateCounterProcessAll) {
        process( false );
    };

    TEST(approximateCounterTerminateEarly) {
        process( true );
    }
};

#endif

}
}

#endif
// vim: syntax=cpp tabstop=4 shiftwidth=4 expandtab
