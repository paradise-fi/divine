// -*- C++ -*- (c) 2012 Petr Rockai <me@mornfall.net>,
//                 2011 Tomáš Janoušek <tomi@nomi.cz>

#include <mutex>
#include <divine/toolkit/shmem.h>

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
    typedef SpinLock Mutex;
    Mutex m;
    bool empty;
    std::deque< T > q;

    LockedQueue( void ) : empty( true ) {}

    void push( const T &x ) {
	std::lock_guard< Mutex > lk( m );
	q.push_back( x );
	empty = false;
    }

    void push( T &&x ) {
	std::lock_guard< Mutex > lk( m );
	q.push_back( std::move( x ) );
	empty = false;
    }

    /**
     * Pops a whole chunk, to be processed by one thread as a whole.
     */
    T pop() {
        T ret = T();

        /* Prevent threads from contending for a lock if the queue is empty. */
        if ( empty )
            return ret;

	std::lock_guard< Mutex > lk( m );

        if ( q.empty() )
            return ret;

        ret = std::move( q.front() );
        q.pop_front();

        if ( q.empty() )
            empty = true;

        return ret;
    }

    LockedQueue( const LockedQueue & ) = delete;
    LockedQueue &operator=( const LockedQueue & ) = delete;
};

}

#endif
