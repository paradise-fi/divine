// -*- C++ -*- (c) 2007 Petr Rockai <me@mornfall.net>

#include <divine/legacy/system/dve/dve_explicit_system.hh>
#include <divine/legacy/system/bymoc/bymoc_explicit_system.hh>

#ifndef DIVINE_GENERATOR_H
#define DIVINE_GENERATOR_H

namespace divine {
namespace generator {

template< typename _State, typename system_t >
struct Common {
    typedef _State State;
    bool m_succsActive;
    succ_container_t m_succs;
    int m_succRes;
    system_t m_system;

    struct Successors {
        int current;
        Common< State, system_t > *m_generator;

        int result() {
            return m_generator->m_succRes;
        }

        bool empty() {
            return current == m_generator->m_succs.size();
        }

        State head() {
            return m_generator->m_succs[ current ];
        }

        Successors tail() {
            Successors s = *this;
            ++ s.current;
            return s;
        }

        Successors( Common &g ) : current( 0 ), m_generator( &g ) {}
    };

    Successors successors( State s ) {
        m_succRes = m_system.get_succs( s.state(), m_succs );
        return Successors( *this );
    }

    State initial() {
        return m_system.get_initial_state();
    }

    void setAllocator( StateAllocator *a ) {
        m_system.setAllocator( a );
    }

    void read( std::string path ) {
        m_system.read( path.c_str() );
    }

    void print_state( State s, std::ostream &o = std::cerr ) {
        m_system.print_state( s.state(), o );
    }

    bool is_accepting( State s ) {
        return m_system.is_accepting( s.state() );
    }

    explicit_system_t *legacy_system() {
        return &m_system;
    }
};

template< typename _State >
struct Dve : Common< _State, dve_explicit_system_t >
{};

template< typename _State >
struct Bymoc : Common< _State, bymoc_explicit_system_t >
{};

template< typename _State >
struct Dummy {
    typedef _State State;
    StateAllocator *m_alloc;

    static uint64_t data( State st ) {
        return *reinterpret_cast< uint64_t * >( st.data() );
    }

    static void setData( State st, uint64_t d ) {
        *reinterpret_cast< uint64_t * >( st.data() ) = d;
    }

    void setAllocator( StateAllocator *a ) { m_alloc = a; }

    State initial() {
        State st = m_alloc->new_state( 8 );
        setData( st, 0 );
        return st;
    }

    struct Successors {
        divine::StateAllocator *alloc;
        State from;
        int nth;
        Successors() : from( 0 ), nth( 1 ) {}

        int result() {
            return 0;
        }

        bool empty() {
            return nth > 6;
        }

        Successors tail() {
            Successors s = *this;
            s.nth ++;
            return s;
        }

        State head() {
            State ret = alloc->new_state( 8 );
            if ( nth <= 4 ) {
                setData( ret, data( from ) > (8000 * 8000) ?
                         data( from ) / 2 : data( from ) * 4 + nth );
            } else {
                setData( ret, data( from ) / 4 + nth );
            }
            return ret;
        }
    };

    Successors successors( State st ) {
        Successors ret;
        ret.alloc = m_alloc;
        ret.from = st;
        return ret;
    }

    bool is_accepting( State s ) {
        return false;
    }

    void print_state( State st ) {
        std::cout << data( st ) << std::endl;
    }

    void read( std::string ) {}

    explicit_system_t *legacy_system() {
        return 0;
    }
};

}
}

#endif
