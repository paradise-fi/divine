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

        //std::cerr << "---- thread " << who << " entering -------" << std::endl;

        bool done = true;

        Set locked;
        done = true;

        for ( MutexIterator i = m_mutexes.begin(); i != m_mutexes.end(); ++i )
        {
            if ( i->first == who ) {
                //std::cerr << "got ourselves at " << i->first << std::endl;
                continue;
            }
            if ( !i->second.trylock() ) {
                //std::cerr << "failed to get at " << i->first << std::endl;
                done = false;
            } else {
                locked.insert( i->first );
                //std::cerr << "locked out " << i->first << std::endl;
            }
        }

        // we are now holding whatever we could get at

        Set busy;

        // now check that there's no work left in the system
        if ( done ) {
            for ( MutexIterator i = m_mutexes.begin(); i != m_mutexes.end(); ++i )
            {
                if ( !i->first->isIdle() ) {
                    busy.insert( i->first );
                    //std::cerr << "thread " << i->first << " busy" << std::endl;
                    done = false;
                } else {
                    //std::cerr << "ok, thread "
                    // << i->first << " idle" << std::endl;
                }
            }
        }

        assert( m_sleeping >= locked.size() );

        // we drop all locks but the first one (on which everyone's preying)
        for ( typename Set::iterator i = locked.begin(); i != locked.end(); ++i ) {
            //std::cerr << "unlocking " << *i << std::endl;
            mutex( *i ).unlock();
        }

        if ( done ) {
            //std::cerr << "---- thread " << who << " wins ------" << std::endl;
            m_done = true; // mark.

            // signal everyone so they get past their sleep
            for ( typename ConditionMap::iterator i = m_conditions.begin();
                  i != m_conditions.end(); ++i ) {
                if ( i->first == who )
                    continue;
                i->second.signal();
            }
        } else {
            // now something failed, let's wake up all sleepers with work waiting
            for ( typename ConditionMap::iterator i = m_conditions.begin();
                  i != m_conditions.end(); ++i ) {
                if ( i->first->isIdle() )
                    continue;
                //std::cerr << "waking up " << i->first << "..." << std::endl;
                i->second.signal(); // wake up that thread
            }

            // and if we have nothing to do ourselves, go to bed...
            if ( who->isIdle() ) {
                //std::cerr << m_sleeping << " thread(s) sleeping, "
                // << locked.size() << " locked" << std::endl;
                if ( m_sleeping < m_expect - 1 ) {
                    //std::cerr << "thread " << who << " going to bed..." << std::endl;
                    ++ m_sleeping;
                    __l.drop();
                    condition( who ).wait( mutex( who ) );
                    __l.reclaim();
                    //std::cerr << who << " woke up" << std::endl;
                    -- m_sleeping;
                } else {
                    //std::cerr << who << " is the last man standing" << std::endl;
                    __l.setYield( true );
                    // we are the last thread to be awake
                }
            } else {
                //std::cerr << who << " still busy, continuing" << std::endl;
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
        m_expect = 0;
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
