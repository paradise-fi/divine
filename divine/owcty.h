// -*- C++ -*- (c) 2007 Petr Rockai <me@mornfall.net>

#include <divine/controller.h>
#include <divine/visitor.h>
#include <divine/observer.h>
#include <divine/threading.h>
#include <divine/legacy/system/path.hh>
#include <divine/report.h>

#ifndef DIVINE_OWCTY_H
#define DIVINE_OWCTY_H

namespace divine {
namespace algorithm {

template< typename _Setup >
struct Owcty : Algorithm
{

    struct ImplCommon {
        Owcty *m_owcty;
        void setOwcty( Owcty *o ) { m_owcty = o; }
        Owcty &owcty() { return *m_owcty; }
        size_t &owctySize() { return owcty().m_size; }
    };

    template< typename Bundle, typename Self >
    struct ReachabilityImpl :
        ImplCommon,
        observer::Common< Bundle, ReachabilityImpl< Bundle, Self > >
    {
        typedef typename Bundle::State State;
        typename Bundle::Visitor &visitor() { return m_controller.visitor(); }

        typename Bundle::Controller &m_controller;
        size_t m_size;

        void expanding( State st )
        {
            assert( st.extension().predCount > 0 );
            assert( st.extension().inS );
            ++ m_size;
        }

        State transition( State f, State t )
        {
            ++ t.extension().predCount;
            t.extension().inS = true;
            if ( t.extension().inF && f.valid() )
                return ignore( t );
            else
                return follow( t );
        }

        ReachabilityImpl( typename Bundle::Controller &controller )
            : m_controller( controller ), m_size( 0 )
        {
        }

        void parallelTermination() {
            zeroFlags( visitor().storage(), false );
            this->owcty().swap(
                m_controller,
                this->owcty().elim.worker( m_controller.id() ) );
        }

        void serialTermination() {
            this->owctySize() += m_size;
            m_size = 0;
        }

    };

    template< typename Bundle, typename Self >
    struct InitializeImpl :
        ImplCommon,
        observer::Common< Bundle, InitializeImpl< Bundle, Self > >
    {
        typedef typename Bundle::State State;
        typename Bundle::Controller &m_controller;
        size_t m_size;

        typename Bundle::Visitor &visitor() { return m_controller.visitor(); }
        typename Bundle::Generator &system() { return visitor().sys; }

        uintptr_t id( State t ) {
            return reinterpret_cast< uintptr_t >( t.ptr );
        }

        State transition( State from, State to )
        {
            if ( !to.extension().parent.valid() )
                to.extension().parent = from;
            if ( from.valid() ) {
                uintptr_t fromMap = from.extension().map;
                uintptr_t fromId = id( from );
                if ( fromMap > to.extension().map )
                    to.extension().map = fromMap;
                if ( system().is_accepting( from ) )
                    if ( fromId > to.extension().map )
                        to.extension().map = fromId;
                if ( id( to ) == fromMap ) {
                    this->owcty().mapCycleFound( to );
                    m_controller.pack().terminate();
                }
            }
            return follow( to );
        }

        void expanding( State st )
        {
            st.extension().predCount = 0;
            if ( system().is_accepting( st ) ) {
                st.extension().inF = st.extension().inS = true;
            } else {
                st.extension().inF = st.extension().inS = false;
            }
        }

        InitializeImpl( typename Bundle::Controller &controller )
            : m_controller( controller )
        {
        }

        void parallelTermination() {
            m_size = swapAndQueue(
                m_controller,
                this->owcty().reach.worker( m_controller.id() ),
                this->owcty().elim.worker( m_controller.id() ) );
        }

        void serialTermination() {
            this->owctySize() += m_size;
        }
    };

    template< typename Bundle, typename Self >
    struct EliminationImpl :
        ImplCommon,
        observer::Common< Bundle, EliminationImpl< Bundle, Self > >
    {
        typedef typename Bundle::State State;
        typename Bundle::Controller &m_controller;
        size_t m_size;

        typename Bundle::Visitor &visitor() { return m_controller.visitor(); }
        typename Bundle::Generator &system() { return visitor().sys; }

        void expanding( State st )
        {
            assert( st.extension().predCount == 0 );
            st.extension().inS = false;
        }

        State transition( State f, State t )
        {
            // assert( m_controller.owner( t ) == m_controller.id() );
            assert( t.valid() );
            assert( !visitor().seen( t ) );
            assert( t.extension().inS );
            assert( t.extension().predCount >= 1 );
            -- t.extension().predCount;
            // we follow a transition only if the target state is going to
            // be eliminated
            if ( t.extension().predCount == 0 )
                return follow( t );
            else
                return ignore( t );
        }

        EliminationImpl( typename Bundle::Controller &c )
            : m_controller( c )
        {
        }

        void parallelTermination() {
            m_size = swapAndQueue(
                m_controller,
                this->owcty().reach.worker( m_controller.id() ),
                m_controller );
        }

        void serialTermination() {
            this->owctySize() += m_size;
        }

    };

    template< typename Bundle, typename Self >
    struct FindCycleImpl :
        ImplCommon,
        observer::Common< Bundle, FindCycleImpl< Bundle, Self > >
    {
        typedef typename Bundle::State State;

        typename Bundle::Controller &m_controller;
        typename Bundle::Visitor &visitor() { return m_controller.visitor(); }

        State transition( State from, State to )
        {
            if ( !to.extension().cycle.valid() ) {
                to.extension().cycle = from;
            }
            if ( from.valid() && to == this->owcty().m_cycleCandidate ) {
                // found cycle
                to.extension().cycle = from;
                this->owcty().m_cycleFound = true;
                return ignore( to );
            }
            return follow( to );
        }

        FindCycleImpl( typename Bundle::Controller &c )
            : m_controller( c )
        {
        }
    };

    struct Extension {
        State< Extension > parent, cycle;
        size_t predCount;
        uintptr_t map;
        bool inS:1;
        bool inF:1;
    };

    typedef Finalize< InitializeImpl > Initialize;
    typedef Finalize< ReachabilityImpl > Reachability;
    typedef Finalize< EliminationImpl > Elimination;
    typedef Finalize< FindCycleImpl > FindCycle;

    typedef typename BundleFromSetup<
        _Setup, Initialize, Extension >::T InitBundle;
    typedef typename BundleFromSetup<
        _Setup, Reachability, Extension >::T ReachBundle;
    typedef typename BundleFromSetup<
        _Setup, Elimination, Extension >::T ElimBundle;
    typedef typename BundleFromSetup<
        _Setup, FindCycle, Extension >::T FindBundle;

    typedef typename InitBundle::State State;

    template< typename A, typename B >
    static void swap( A &a, B &b )
    {
        std::swap( a.visitor().storage(), b.visitor().storage() );
    }

    template< typename A, typename B, typename C >
    static int swapAndQueue( A &a, B &b, C &c )
    {
        int ret = 0;
        typename A::Storage &stor = a.visitor().storage();
        assert( !b.checkForWork() );
        assert( !c.checkForWork() );
        for ( size_t i = 0; i < stor.table().size(); ++i ) {
            State st = stor.table()[ i ].key;
            // assert( a.owner( st ) == a.id() );
            if ( st.valid() && st.extension().inS && st.extension().inF ) {
                b.queue( st );
                c.queue( st );
                ++ ret;
            }
        }

        zeroFlags( a.visitor().storage(), true );

        swap( a, b );

        return ret;
    }

    template< typename Storage >
    static void zeroFlags( Storage &stor, bool pcount )
    {
        for ( size_t j = 0; j < stor.table().size(); ++j ) {
            typename Storage::State st = stor.table()[ j ].key;
            if ( st.valid() ) {
                st.setFlags( 0 );
                if ( pcount )
                    st.extension().predCount = 0;
            }
        }
    }

    threads::Pack< typename InitBundle::Controller > init;
    threads::Pack< typename ReachBundle::Controller > reach;
    threads::Pack< typename ElimBundle::Controller > elim;
    threads::Pack< typename FindBundle::Controller > find;

    size_t m_size;

    State m_cycleCandidate;
    State m_mapCycleState;
    bool m_cycleFound;

    Owcty( Config &c ) : Algorithm( c ), init( c ), reach( c ),
                         elim( c ), find( c ),
                         m_size( 0 )
    {
        init.initialize();
        reach.initialize();
        elim.initialize();
        find.initialize();

        for ( int i = 0; i < init.workerCount(); ++i ) {
            init.worker( i ).observer().setOwcty( this );
            reach.worker( i ).observer().setOwcty( this );
            elim.worker( i ).observer().setOwcty( this );
            find.worker( i ).observer().setOwcty( this );
        }
    }

    void mapCycleFound( State st ) {
        m_mapCycleState = st;
    }

    State findCounterexampleFrom( State st ) {
        m_cycleCandidate = st;
        find.blockingVisit( st );
        if ( m_cycleFound )
            return st;
        return State();
    }

    template< typename T >
    State iterateCandidates( T &worker )
    {
        typedef typename T::Visitor::Storage Storage;
        Storage &s = worker.visitor().storage();
        for ( int j = 0; j < s.table().size(); ++j ) {
            State st = s.table()[ j ].key;
            if ( !st.valid() )
                continue;
            if ( worker.visitor().seen( st ) || !st.extension().inF )
                continue;
            if ( ( st = findCounterexampleFrom( st ) ).valid() )
                return st;
        }
        return State();
    }

    State counterexample() {
        State st;

        m_cycleFound = false;

        for ( int i = 0; i < find.workerCount(); ++i ) {
            swap( reach.worker( i ), find.worker( i ) );
            zeroFlags( find.worker( i ).visitor().storage(), false );
        }

        for ( int i = 0; i < find.workerCount(); ++i ) {
            if ( !st.valid() )
                st = iterateCandidates( find.worker( i ) );
        }

        assert( st.valid() );
        assert( st.extension().inF );

        return st;
    }

    void printCounterexample( State st ) {
        State s = st;
        path_t ce;
        explicit_system_t * sys =
            init.anyWorker().observer().system().legacy_system();
        if (!sys) {
            std::cerr << "ERROR: Unable to produce counterexample" << std::endl;
            return;
        }

        ce.set_system( sys );
        ce.setAllocator( init.anyWorker().storage().newAllocator() );
        do {
            s = s.extension().cycle;
            ce.push_front( s.state() );
            assert( s.valid() );
        } while ( s != st );
        ce.mark_cycle_start_front();
        while ( s != State( init.anyWorker().visitor().sys.initial() ) ) {
            s = s.extension().parent;
            ce.push_front( s.state() );
        }
        ce.write_trans(this->config().trailStream());
    }

    void printSize() {
        if ( m_mapCycleState.valid() )
            std::cerr << "   (MAP: cycle found)" << std::endl << std::flush;
        else
            std::cerr << "   |S| = " << m_size << std::endl << std::flush;
    }

    void printIteration( int i ) {
        std::cerr << "------------- iteration " << i
                  << " ------------- " << std::endl;
    }

    Result run()
    {
        size_t oldsize = 0;

        std::cerr << " initialize...\t\t" << std::flush;
        m_size = 0;
        init.blockingVisit();
        printSize();

        int iter = 0;

        if ( !m_mapCycleState.valid() ) {
            do {
                oldsize = m_size;
                
                printIteration( iter );
                
                std::cerr << " reachability...\t" << std::flush;
                m_size = 0;
                reach.blockingRun();
                printSize();
                
                std::cerr << " elimination & reset...\t" << std::flush;
                m_size = 0;
                elim.blockingRun();
                printSize();
                
                ++iter;
                
            } while ( oldsize != m_size && m_size != 0 );
        }

        bool valid = m_mapCycleState.valid() ? false : ( m_size == 0 );
        std::cerr << " ===================================== " << std::endl
                  << ( valid ?
                     "       Accepting cycle NOT found       " :
                     "         Accepting cycle FOUND         " )
                  << std::endl
                  << " ===================================== " << std::endl;

        // counterexample generation

        if ( !valid && this->config().generateCounterexample() ) {
            std::cerr << " generating counterexample...\t" << std::flush;
            State st;
            if ( m_mapCycleState.valid() )
                st = findCounterexampleFrom( m_mapCycleState );
            else
                st = counterexample();
            std::cerr << "   done" << std::endl;

            printCounterexample( st );
        }

        Result res;
        res.ltlPropertyHolds = valid ? Result::Yes : Result::No;
        res.fullyExplored = m_mapCycleState.valid() ? Result::No : Result::Yes;
        return res;
    }

};

}
}

#endif
