// -*- C++ -*- (c) 2008 Petr Rockai <me@mornfall.net>
//             (c) 2014 Vladimír Štill <xstill@fi.muni.cz>

#include <map>
#include <mutex>
#include <condition_variable>
#include <brick-assert.h>

#ifndef DIVINE_BARRIER_H
#define DIVINE_BARRIER_H

namespace divine {

// You can usually use Barrier< Terminable > and derive your threads from this
// class. This is however not a requirement of Barrier itself.
struct Terminable {
    virtual bool workWaiting() = 0;
    virtual bool isBusy() = 0;
    bool sleeping;

    Terminable() : sleeping( false ) {}
};

template< typename T >
struct Barrier {
    struct MutexLock {
        MutexLock( std::mutex &mutex ) : _guard( mutex ), _yield( false ) { }
        ~MutexLock() {
            _guard.unlock();
            if ( _yield )
                std::this_thread::yield();
        }

        void lock() { _guard.lock(); }
        void unlock() { _guard.unlock(); }

        void setYield() { _yield = true; }

      private:
        std::unique_lock< std::mutex > _guard;
        bool _yield;
    };

    typedef std::map< T *, std::shared_ptr< std::condition_variable_any > > ConditionMap;
    typedef std::map< T *, std::shared_ptr< std::mutex > > MutexMap;

    MutexMap m_mutexes;
    std::mutex m_globalMutex;
    ConditionMap m_conditions;
    int m_sleeping;

    typedef std::set< T * > Set;
    volatile bool m_done;

    int m_regd, m_expect;

    void wakeup( T *who ) {
        if ( who->sleeping )
            condition( who ).notify_one();
    }

    std::mutex &mutex( T *t ) {
        return *m_mutexes[ t ];
    }

    std::condition_variable_any &condition( T *t ) {
        return *m_conditions[ t ];
    }

    bool maybeIdle( T *who, bool really, bool sleep ) {
        ASSERT( m_expect );
        if ( m_regd < m_expect )
            return false;

        MutexLock __l( m_globalMutex );

        m_done = false; // reset
        bool done = true;
        bool who_is_ours = false;

        Set locked, busy;
        done = true;

        for ( auto &i : m_mutexes ) {
            if ( i.first == who ) {
                who_is_ours = true;
                continue;
            }
            if ( !i.second->try_lock() ) {
                done = false;
            } else {
                locked.insert( i.first );
            }
        }

        ASSERT( who_is_ours );
        (void)who_is_ours;

        // we are now holding whatever we could get at; let's check that
        // there's no work left in the system
        if ( done ) {
            for ( auto &i : m_mutexes )
            {
                if ( (i.first != who && i.first->isBusy()) ||
                     i.first->workWaiting() )
                {
                    busy.insert( i.first );
                    done = false;
                }
            }
        }

        // certainly, there are at least as many sleepers as we could lock
        // out... there may be more, since they might have let gone of the
        // global mutex, but still haven't arrived to the condition wait
        ASSERT_LEQ( int( locked.size() ), m_sleeping );

        // we drop all locks (but we hold on to the global one)
        for ( auto &i : locked )
            mutex( i ).unlock();

        if ( done ) {
            if ( !really )
                return true;

            m_done = true; // mark.

            // signal everyone so they get past their sleep
            for ( auto &i : m_conditions ) {
                if ( i.first == who )
                    continue;
                i.second->notify_one();
            }
        } else {
            // something failed, let's wake up all sleepers with work waiting
            for ( auto &i : m_conditions ) {
                if ( !i.first->workWaiting() )
                    continue;
                i.second->notify_one(); // wake up that thread
            }

            // and if we have nothing to do ourselves, go to bed... for the
            // "last man" check, we don't go to sleep, just return false right
            // away here
            if ( !sleep )
                return false;

            if ( !who->workWaiting() ) {
                if ( m_sleeping < m_expect - 1 ) {
                    ++ m_sleeping;
                    who->sleeping = true;
                    __l.unlock();
                    condition( who ).wait( mutex( who ) );
                    __l.lock();
                    who->sleeping = false;
                    -- m_sleeping;
                } else {
                    // we are the last thread to be awake; we need to yield
                    // after unlocking the global mutex so we avoid starving
                    // the rest of the system (that's trying to wake up and
                    // needs the global mutex for that)
                    __l.setYield();
                }
            }
        }

        if ( m_done && really )
            this->done( who, __l );
        return m_done;
    }

    bool idle( T *who, bool sleep = true ) {
        return maybeIdle( who, true, sleep );
    }

    // Returns true if a call to idle( who ) would return true, but *does not
    // perform the actual termination* (ie. a thread can probe whether the rest
    // of the system is idle, without actually becoming idle itself).
    bool lastMan( T *who, bool sleep = false ) {
        bool r = maybeIdle( who, false, sleep );
        return r;
    }

    void done( T *t ) {
        MutexLock __l( m_globalMutex );
        done( t, __l );
    }

    void done( T *t, MutexLock & ) {
        if ( m_mutexes.count( t ) ) {
            mutex( t ).unlock();
            m_mutexes.erase( t );
            m_conditions.erase( t );
            -- m_regd;
        }
    }

    void started( T *t ) {
        MutexLock __l( m_globalMutex );
        if ( m_mutexes.count( t ) )
            return; // already started, noop
        m_conditions[ t ].reset( new std::condition_variable_any() );
        m_mutexes[ t ].reset( new std::mutex() );
        m_mutexes[ t ]->lock();
        ++ m_regd;
    }

    void clear() {
        m_regd = 0;
        m_done = false;
        m_mutexes.clear();
        m_conditions.clear();
        m_sleeping = 0;
    }

    void setExpect( int n ) {
        m_regd = 0;
        m_expect = n;
    }

    Barrier() {
        clear();
    }
};

}

namespace divine_test {

using namespace divine;

struct TestBarrier {
    struct Thread : brick::shmem::Thread {
        volatile int i;
        int id;
        bool busy;
        TestBarrier *owner;
        bool sleeping;

        Thread() : i( 0 ), id( 0 ), busy( false ), sleeping( false ) {}
        Thread( const Thread & ) = default;

        bool workWaiting() {
            if ( busy )
                return true;
            busy = (i % 3) != 1;
            return busy;
        }
        bool isBusy() { return false; }

        void main() {
            busy = true;
            owner->barrier.started( this );
            while ( true ) {
                // std::cerr << "thread " << this << " iteration " << i << std::endl;
                owner->threads[ rand() % owner->count ].i ++;
                if ( i % 3 == 1 ) {
                    busy = false;
                    if ( owner->barrier.idle( this ) )
                        return;
                }
            }
        }
    };

    Barrier< Thread > barrier;
    std::vector< Thread > threads;
    int count;

    TEST(basic) {
        Thread bp;
        count = 5;
        bp.owner = this;
        bp.i = 0;
        threads.resize( count, bp );
        barrier.setExpect( count );
        for ( int i = 0; i < count; ++i ) {
            threads[ i ].id = i;
            threads[ i ].start();
        }
        for ( int i = 0; i < count; ++i ) {
            threads[ i ].join();
            ASSERT_EQ( threads[ i ].i % 3, 1 );
        }
        barrier.clear();
    }

};

}

#endif
