// -*- C++ -*- (c) 2008 Petr Rockai <me@mornfall.net>

#include <wibble/sys/mutex.h>
#include <map>

#ifndef DIVINE_BARRIER_H
#define DIVINE_BARRIER_H

namespace divine {

template< typename T >
struct Barrier {
    typedef wibble::sys::Mutex Mutex;
    typedef wibble::sys::MutexLock MutexLock;

    typedef std::map< T *, wibble::sys::Condition > ConditionMap;
    typedef std::map< T *, Mutex > MutexMap;

    typedef typename ConditionMap::iterator ConditionIterator;
    typedef typename MutexMap::iterator MutexIterator;

    MutexMap m_mutexes;
    Mutex m_globalMutex;
    ConditionMap m_conditions;
    int m_sleeping;

    typedef std::set< T * > Set;
    volatile bool m_done;

    int m_regd, m_expect;

    Mutex &mutex( T *t ) {
        return m_mutexes[ t ];
    }

    wibble::sys::Condition &condition( T *t ) {
        return m_conditions[ t ];
    }

    bool idle( T *who ) {
        assert( m_expect );
        if ( m_regd < m_expect )
            return false;

        MutexLock __l( m_globalMutex );

        bool done = true;
        bool who_is_ours = false;

        Set locked, busy;
        done = true;

        for ( MutexIterator i = m_mutexes.begin(); i != m_mutexes.end(); ++i )
        {
            if ( i->first == who ) {
                who_is_ours = true;
                continue;
            }
            if ( !i->second.trylock() ) {
                done = false;
            } else {
                locked.insert( i->first );
            }
        }

        assert( who_is_ours );

        // we are now holding whatever we could get at; let's check that
        // there's no work left in the system
        if ( done ) {
            for ( MutexIterator i = m_mutexes.begin(); i != m_mutexes.end(); ++i )
            {
                if ( !i->first->isIdle() ) {
                    busy.insert( i->first );
                    done = false;
                }
            }
        }

        // certainly, there are at least as many sleepers as we could lock
        // out... there may be more, since they might have let gone of the
        // global mutex, but still haven't arrived to the condition wait
        assert( m_sleeping >= locked.size() );

        // we drop all locks (but we hold on to the global one)
        for ( typename Set::iterator i = locked.begin(); i != locked.end(); ++i ) {
            mutex( *i ).unlock();
        }

        if ( done ) {
            m_done = true; // mark.

            // signal everyone so they get past their sleep
            for ( typename ConditionMap::iterator i = m_conditions.begin();
                  i != m_conditions.end(); ++i ) {
                if ( i->first == who )
                    continue;
                i->second.signal();
            }
        } else {
            // something failed, let's wake up all sleepers with work waiting
            for ( typename ConditionMap::iterator i = m_conditions.begin();
                  i != m_conditions.end(); ++i ) {
                if ( i->first->isIdle() )
                    continue;
                i->second.signal(); // wake up that thread
            }

            // and if we have nothing to do ourselves, go to bed...
            if ( who->isIdle() ) {
                if ( m_sleeping < m_expect - 1 ) {
                    ++ m_sleeping;
                    __l.drop();
                    condition( who ).wait( mutex( who ) );
                    __l.reclaim();
                    -- m_sleeping;
                } else {
                    // we are the last thread to be awake; we need to yield
                    // after unlocking the global mutex so we avoid starving
                    // the rest of the system (that's trying to wake up and
                    // needs the global mutex for that)
                    __l.setYield( true );
                }
            }
        }

        if ( m_done )
            this->done( who );
        return m_done;
    }

    void done( T *t ) {
        MutexLock __l( m_globalMutex );
        if ( m_mutexes.count( t ) ) {
            mutex( t ).unlock();
            m_mutexes.erase( t );
        }
    }

    void started( T *t ) {
        MutexLock __l( m_globalMutex );
        m_conditions[ t ] = wibble::sys::Condition();
        m_mutexes[ t ].lock();
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

    Barrier() : m_globalMutex( true )
    {
        clear();
    }
};

}

#endif
