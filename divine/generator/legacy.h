// -*- C++ -*- (c) 2007, 2008, 2009 Petr Rockai <me@mornfall.net>

#ifdef O_LEGACY
#include <divine/legacy/system/dve/dve_explicit_system.hh>
#include <divine/legacy/system/dve/dve_prob_explicit_system.hh>
#include <divine/legacy/system/bymoc/bymoc_explicit_system.hh>
#include <divine/legacy/por/por.hh>

#include <sstream>
#include <stdexcept>

#include <divine/generator/common.h>
#include <divine/graph/datastruct.h> // XXX for safe_delete

#ifndef DIVINE_GENERATOR_LEGACY_H
#define DIVINE_GENERATOR_LEGACY_H

namespace divine {
namespace generator {

template< typename _State, typename system_t, typename container_t >
struct LegacyCommon : Common< _State > {
    typedef _State State;
    typedef State Node;
    typedef typename Common< _State >::Label Label;
    typedef generator::Common< _State > Common;
    typedef LegacyCommon< _State, system_t, container_t > Graph;

    std::string file;
    system_t *m_system;
    por_t *m_por;

    /// Returns successors of state s
    template< typename Yield >
    void successors( State s, Yield yield ) {
        assert( s.valid() );
        if ( legacy_system()->is_erroneous( this->alloc.legacy_state( s ) ) )
            return;

        container_t succs;
        state_t legacy = this->alloc.legacy_state( s );
        legacy_system()->get_succs( legacy, succs );
        for ( auto i = succs.begin(); i != succs.end(); ++i )
            yield( this->alloc.unlegacy_state( i->getState() ), Label() );
    }

    /// Finds successors of transitions involving specified process (or every process except that one)

    template< typename Yield >
    void processSuccessors( State s, Yield yield, int pid, bool include = true ) {
        assert( s.valid() );
        // works only with dve_explicit_system_t
        dve_explicit_system_t* system = dynamic_cast< dve_explicit_system_t* >( legacy_system() );

        state_t legacy = this->alloc.legacy_state( s );
        enabled_trans_container_t enabled_trans( *system );
        legacy_system()->get_enabled_trans( legacy, enabled_trans );
        unsigned proc_gid = pid;
        if ( system->get_with_property() && proc_gid >= system->get_property_gid() )
            proc_gid++; // skip property process
        for ( std::size_t i=0; i != enabled_trans.size(); i++ ) {
            dve_transition_t * const trans = system->get_sending_or_normal_trans( enabled_trans[i] );
	        dve_transition_t * const recv_trans = system->get_receiving_trans( enabled_trans[i] );
	        // test if given process participates on this transition
	        bool involved = trans->get_process_gid() == proc_gid ||
                    (recv_trans && recv_trans->get_process_gid() == proc_gid);
            if ( involved == include ) {
                state_t succ;
                system->get_ith_succ( legacy, i, succ );
                yield( this->alloc.unlegacy_state( succ ), Label() );
            }
        }
    }

    /// Count processes and ignore property process
    int processCount() {
        int count = legacy_system()->get_process_count();
        return ( legacy_system()->get_with_property() ? count - 1 : count );
    }

    /// Partial order reduction
    por_t &por() {
        if ( !m_por ) {
            m_por = new por_t;
            m_por->init( legacy_system() );
            m_por->set_choose_type( POR_SMALLEST ); // XXX
        }
        return *m_por;
    }

    template< typename Yield >
    void ample( State s, Yield yield ) {
        assert( s.valid() );
        if ( legacy_system()->is_erroneous( this->alloc.legacy_state( s ) ) )
            return;

        container_t succs;
        state_t legacy = this->alloc.legacy_state( s );
        size_t proc_gid;
        por().ample_set_succs( legacy, succs, proc_gid );
        for ( auto i = succs.begin(); i != succs.end(); ++i )
            yield( this->alloc.unlegacy_state( i->getState() ), Label() );
    }

    /// Returns the initial state
    State initial() {
        return this->alloc.unlegacy_state( legacy_system()->get_initial_state() );
    }

    /// Enques the initial state
    template< typename Q >
    void queueInitials( Q &q ) {
        q.queue( State(), initial(), Label() );
    }

    /// Reads the state space from file
    void read( std::string path ) {
        file = path;
        legacy_system(); // force
    }

    void print_state( State s, std::ostream &o = std::cerr ) {
        legacy_system()->print_state( this->alloc.legacy_state( s ), o );
    }

    /// Is state s in accepting set?
    bool isAccepting( State s ) {
        return legacy_system()->is_accepting( this->alloc.legacy_state( s ) );
    }

    /// Is state s in acc_group set or accepting set of acc_group pair?
    bool isInAccepting( State s, const size_int_t acc_group ) {
        return legacy_system()->is_accepting( this->alloc.legacy_state( s ), acc_group, 1 );
    }

    /// Is state s in rejecting set of acc_group pair?
    bool isInRejecting( State s, const size_int_t acc_group ) {
        return legacy_system()->is_accepting( this->alloc.legacy_state( s ), acc_group, 2 );
    }

    /// Number of sets/pairs of acceptance condition
    unsigned acceptingGroupCount() {
        if ( !legacy_system()->get_with_property() ) return 0;
        process_t* propertyProcess = legacy_system()->get_property_process();
        return dynamic_cast< dve_process_t* >( propertyProcess )->get_accepting_group_count();
    }

    /// Type of acceptance condition
    PropertyType propertyType() {
        if ( legacy_system()->get_with_property() ) {
            process_t* propertyProcess = legacy_system()->get_property_process();
            return static_cast< PropertyType >( 
                dynamic_cast< dve_process_t* >( propertyProcess )->get_accepting_type() );
        }
        else
            return AC_None;
    }

    bool isGoal( State ) { return false; } // XXX
    std::string showNode( State s ) {
        if ( !s.valid() )
            return "[]";
        std::stringstream o;
        legacy_system()->print_state( this->alloc.legacy_state( s ), o );
        return o.str();
    }

    std::string showTransition( State from, State to ) {
        if ( !from.valid() || !to.valid() )
            return "";
        std::stringstream o;
        legacy_system()->print_transition( this->alloc.legacy_state( from ),
                                           this->alloc.legacy_state( to ), o );
        return o.str();
    }

    system_t *legacy_system() {
        static wibble::sys::Mutex m;
        if ( !m_system ) {
            wibble::sys::MutexLock _lock( m );
            m_system = new system_t;
            m_system->setAllocator( &this->alloc );
            if ( !file.empty() ) {
                if ( legacy_system()->read( file.c_str() ) ) // don't ask.
                    throw std::runtime_error( "Error reading input model." );
            }
        }
        return m_system;
    }

    void release( State s ) {
        s.free( this->pool() );
    }

    LegacyCommon &operator=( const LegacyCommon &other ) {
        Common::operator=( other );
        file = other.file;
        safe_delete( m_system );
        safe_delete( m_por );
        legacy_system(); // FIXME, we force read here to keep
                         // dve_explicit_system::read() from happening in
                         // multiple threads at once, no matter the mutex...
        return *this;
    }

    LegacyCommon( const LegacyCommon &other )
        : Common( other ), file( other.file ), m_system( 0 ), m_por( 0 ) {}
    LegacyCommon() : m_system( 0 ), m_por( 0 ) {}

    ~LegacyCommon() {
        safe_delete( m_system );
        safe_delete( m_por );
    }
};

struct LegacyDve : LegacyCommon< Blob, dve_explicit_system_t, succ_container_t >
{};

struct LegacyProbDve : LegacyCommon< Blob, dve_prob_explicit_system_t, prob_succ_container_t > 
{};

struct LegacyBymoc : LegacyCommon< Blob, bymoc_explicit_system_t, succ_container_t >
{};

}
}

#endif
#endif
