// -*- C++ -*- (c) 2012 Petr Rockai <me@mornfall.net>,
//                 2011 Tomáš Janoušek <tomi@nomi.cz>,
//                 2014 Vladimír Štill <xstill@fi.muni.cz>

#include <deque>
#include <mutex>
#include <brick-shmem.h>

#ifndef DIVINE_LOCKEDQUEUE_H
#define DIVINE_LOCKEDQUEUE_H

namespace divine {

/**
 * A very simple spinlock-protected queue based on std::deque of std::vector
 * chunks. For automatic chunk building and pushing, see Chunker. It appears
 * to be fast enough because it's accessed very rarely and it needs to
 * allocate or free memory even less often.
 */
// note: std::deque of int, not of std:vector
template < typename T >
struct LockedQueue {
    typedef brick::shmem::SpinLock Mutex;
    Mutex m;
    brick::shmem::WeakAtomic< bool > _empty;
    std::deque< T > q;

    LockedQueue( void ) : _empty( true ) {}

    bool empty() const { return _empty; }

    void push( const T &x ) {
        std::lock_guard< Mutex > lk( m );
        q.push_back( x );
        _empty = false;
    }

    void push( T &&x ) {
        std::lock_guard< Mutex > lk( m );
        q.push_back( std::move( x ) );
        _empty = false;
    }

    /**
     * Pops a whole chunk, to be processed by one thread as a whole.
     */
    T pop() {
        T ret = T();

        /* Prevent threads from contending for a lock if the queue is empty. */
        if ( empty() )
            return ret;

        std::lock_guard< Mutex > lk( m );

        if ( q.empty() )
            return ret;

        ret = std::move( q.front() );
        q.pop_front();

        if ( q.empty() )
            _empty = true;

        return ret;
    }

    void clear() {
        std::lock_guard< Mutex > guard{ m };
        q.clear();
        _empty = true;
    }

    LockedQueue( const LockedQueue & ) = delete;
    LockedQueue &operator=( const LockedQueue & ) = delete;
};

}

#endif
