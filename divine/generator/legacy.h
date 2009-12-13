// -*- C++ -*- (c) 2007, 2008, 2009 Petr Rockai <me@mornfall.net>

#include <divine/legacy/system/dve/dve_explicit_system.hh>
#include <divine/legacy/system/bymoc/bymoc_explicit_system.hh>
#include <sstream>

#include <divine/generator/common.h>

#ifndef DIVINE_GENERATOR_LEGACY_H
#define DIVINE_GENERATOR_LEGACY_H

namespace divine {
namespace generator {

template< typename _State, typename system_t >
struct LegacyCommon : Common {
    typedef _State State;
    typedef State Node;

    std::string file;
    system_t *m_system;

    struct Successors {
        typedef Node Type;
        int current;
        succ_container_t m_succs;
        Node _from;
        LegacyCommon *parent;

        int result() {
            return 0;
        }

        bool empty() const {
            if ( !_from.valid() )
                return true;
            if ( parent->legacy_system()->is_erroneous( parent->alloc.legacy_state( _from ) ) )
                return true;
            return current == m_succs.size();
        }

        Node from() { return _from; }

        State head() const {
            return parent->alloc.unlegacy_state( m_succs[ current ] );
        }

        Successors tail() const {
            Successors s = *this;
            ++ s.current;
            return s;
        }

        Successors() : current( 0 ) {}
    };

    Successors successors( State s ) {
        assert( s.valid() );
        Successors succ;
        succ.parent = this;
        succ._from = s;
        state_t legacy = alloc.legacy_state( s );
        legacy_system()->get_succs( legacy, succ.m_succs );
        return succ;
    }

    State initial() {
        return alloc.unlegacy_state( legacy_system()->get_initial_state() );
    }

    void read( std::string path ) {
        legacy_system()->read( path.c_str() );
        file = path;
    }

    void print_state( State s, std::ostream &o = std::cerr ) {
        legacy_system()->print_state( legacy_state( s ), o );
    }

    bool isAccepting( State s ) {
        return legacy_system()->is_accepting( alloc.legacy_state( s ) );
    }

    bool isDeadlock( State s ) { return false; } // XXX
    bool isGoal( State s ) { return false; } // XXX
    std::string showNode( State s ) {
        if ( !s.valid() )
            return "[]";
        std::stringstream o;
        legacy_system()->print_state( alloc.legacy_state( s ), o );
        return o.str();
    }

    explicit_system_t *legacy_system() {
        if ( !m_system ) {
            m_system = new system_t;
            m_system->setAllocator( &alloc );
            if ( !file.empty() ) {
                m_system->read( file.c_str() );
            }
        }
        return m_system;
    }

    void release( State s ) {
        s.free( pool() );
    }

    LegacyCommon &operator=( const LegacyCommon &other ) {
        file = other.file;
        safe_delete( m_system );
        legacy_system(); // FIXME, we force read here to keep
                         // dve_explicit_system::read() from happening in
                         // multiple threads at once, no matter the mutex...
        return *this;
    }

    LegacyCommon( const LegacyCommon &other )
        : Common( other ), file( other.file ), m_system( 0 ) {}
    LegacyCommon() : m_system( 0 ) {}

    ~LegacyCommon() {
        safe_delete( m_system );
    }
};

template< typename _State >
struct Dve : LegacyCommon< _State, dve_explicit_system_t >
{};

template< typename _State >
struct Bymoc : LegacyCommon< _State, bymoc_explicit_system_t >
{};

typedef Dve< Blob > NDve;
typedef Bymoc< Blob > NBymoc;

}
}

#endif
