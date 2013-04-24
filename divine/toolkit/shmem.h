// -*- C++ -*- (c) 2012 Petr Rockai <me@mornfall.net>,
//                 2011 Tomáš Janoušek <tomi@nomi.cz>


#include <mutex>
#include <atomic>

#ifndef DIVINE_SHMEM_H
#define DIVINE_SHMEM_H

namespace divine {

/**
 * A spinlock implementation.
 *
 * One has to wonder why this is missing from the C++0x STL.
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
        if ( local > 0 ) {
            shared.counter -= local;
            local = 0;
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

    bool isZero() {
        /* user is responsible for calling sync(), this method is called way
         * too often; the counter might drop below zero when reset() is called
         * due to early termination, and a sync() intervenes, substracting a
         * non-zero local approximation */
        return shared.counter <= 0;
    }

    void reset() { shared.counter = 0; }

    ApproximateCounter( const ApproximateCounter &a )
        : shared( a.shared ), local( a.local )
    {}
    ApproximateCounter operator=( const ApproximateCounter & ) = delete;
};

}

#endif
