// -*- C++ -*- (c) 2007 Petr Rockai <me@mornfall.net>

#include <divine/algorithm.h>
#include <divine/controller.h>
#include <divine/visitor.h>
#include <divine/observer.h>
#include <divine/threading.h>
#include <time.h>

#include <divine/report.h>

#ifndef DIVINE_REACHABILITY_H
#define DIVINE_REACHABILITY_H

namespace divine {
namespace algorithm {

template< typename _Setup >
struct Reachability : Algorithm {

    template< typename Bundle, typename Self >
    struct ObserverImpl :
        observer::Common< Bundle, ObserverImpl< Bundle, Self > >
    {
        typedef typename Bundle::State State;
        size_t m_stateCount, m_acceptingCount, m_transitionCount;
        size_t m_errorCount, m_deadlockCount;
        typename Bundle::Controller &m_controller;

        typename Bundle::Visitor &visitor() { return m_controller.visitor(); }
        typename Bundle::Generator &system() { return visitor().sys; }

        void expanding( State st )
        {
            ++m_stateCount;
            if ( system().is_accepting( st ) ) {
                ++ m_acceptingCount;
            }
        }

        void errorState( State st, int err )
        {
            if ( err & SUCC_DEADLOCK ) {
                m_deadlockCount ++;
                if ( m_deadlockCount > 1 ) {
                    return;
                } else {
                    std::cerr << "WARNING: deadlock state reached (at most one deadlock per thread is reported):" << std::endl;
                }
            } else {
                m_errorCount ++;
                if ( m_errorCount > 1 ) {
                    return;
                } else {
                    std::cerr << "WARNING: error state reached (at most one error per thread is reported):" << std::endl;
                }
            }
            system().print_state( st.state() );
        }

        State transition( State f, State t )
        {
            ++ m_transitionCount;
            return follow( t );
        }

        void done()
        {
            // invalid in presence of shared storage
            // assert( m_stateCount == m_controller.storage().table().usage() );
        }

        void printStatistics( std::ostream &str, std::string prefix )
        {
            str << prefix << "states: " << m_stateCount << std::endl;
            str << prefix << "accepting states: " << m_acceptingCount
                << std::endl;
            str << prefix << "transitions: " << m_transitionCount << std::endl;
        }

        ObserverImpl( typename Bundle::Controller &controller )
            : m_stateCount( 0 ), m_acceptingCount( 0 ),
              m_transitionCount( 0 ),
              m_errorCount( 0 ), m_deadlockCount( 0 ),
              m_controller( controller )
        {
        }
    };

    typedef Finalize< ObserverImpl > Observer;

    Reachability( Config &c ) : Algorithm( c )
    {}

    typedef typename BundleFromSetup< _Setup, Observer, Unit >::T Bundle;

    Result run() {
        threads::Pack< typename Bundle::Controller > pack( config() );
        pack.initialize();
        pack.blockingVisit();
        int seen = 0, acc = 0, trans = 0, errs = 0, deadl = 0;
        for ( int i = 0; i < pack.m_threads; ++i ) {
            seen += pack.worker( i ).observer().m_stateCount;
            trans += pack.worker( i ).observer().m_transitionCount;
            acc += pack.worker( i ).observer().m_acceptingCount;
            deadl += pack.worker( i ).observer().m_deadlockCount;
            errs += pack.worker( i ).observer().m_errorCount;
        }
        std::cerr << "seen total of: " << seen << " states (" << acc
                  << " accepting) and " << trans << " transitions" << std::endl;
        std::cerr << "encountered total of " << errs << " errors and "
                  << deadl << " deadlocks" << std::endl;
        Result res;
        res.visited = res.expanded = seen;
        res.deadlocks = deadl;
        res.errors = errs;
        res.fullyExplored = Result::Yes;
        return res;
    }
};

}

}

#endif
