// -*- C++ -*- (c) 2007 Petr Rockai <me@mornfall.net>

#include <divine/legacy/system/dve/dve_explicit_system.hh>
#include <divine/legacy/system/bymoc/bymoc_explicit_system.hh>

#include <divine/stateallocator.h>
#include <divine/blob.h>

#ifndef DIVINE_GENERATOR_H
#define DIVINE_GENERATOR_H

namespace divine {
namespace generator {

extern wibble::sys::Mutex *read_mutex;
inline wibble::sys::Mutex &readMutex() {
    assert( read_mutex );
    return *read_mutex;
}

template< typename _State, typename system_t >
struct Common {
    typedef _State State;
    typedef State Node;

    std::string file;
    system_t *m_system;
    BlobAllocator *alloc;

    struct Successors {
        int current;
        succ_container_t m_succs;
        Node _from;

        int result() {
            return 0;
        }

        bool empty() {
            return current == m_succs.size();
        }

        Node from() { return _from; }

        State head() {
            return State( m_succs[ current ].ptr, true );
        }

        Successors tail() {
            Successors s = *this;
            ++ s.current;
            return s;
        }

        Successors() : current( 0 ) {}
    };

    Successors successors( State s ) {
        Successors succ;
        succ._from = s;
        state_t legacy = legacy_state( s );
        legacy_system()->m_allocator->fixup_state( legacy );
        legacy_system()->get_succs( legacy, succ.m_succs );
        return succ;
    }

    State initial() {
        return Node( legacy_system()->get_initial_state().ptr, true );
    }

    void setAllocator( StateAllocator *a ) {
        alloc = dynamic_cast< BlobAllocator * >( a );
        assert( alloc );
        legacy_system()->setAllocator( alloc );
    }

    int stateSize() {
        dve_explicit_system_t *sys =
            dynamic_cast< dve_explicit_system_t * >( legacy_system() );
        assert( sys );
        return sys->get_space_sum();
    }

    void read( std::string path ) {
        MutexLock __l( readMutex() );
        legacy_system()->read( path.c_str() );
        file = path;
    }

    void print_state( State s, std::ostream &o = std::cerr ) {
        legacy_system()->print_state( legacy_state( s ), o );
    }

    bool is_accepting( State s ) {
        return legacy_system()->is_accepting( legacy_state( s ) );
    }

    explicit_system_t *legacy_system() {
        if ( !m_system ) {
            MutexLock __l( readMutex() );
            m_system = new system_t;
            m_system->setAllocator( alloc = new BlobAllocator() );
            if ( !file.empty() ) {
                m_system->read( file.c_str() );
            }
        }
        return m_system;
    }

    void release( State s ) {
        s.free( alloc->alloc() );
    }

    Common &operator=( const Common &other ) {
        file = other.file;
        m_system = 0;
        legacy_system(); // FIXME, we force read here to keep
                         // dve_explicit_system::read() from happening in
                         // multiple threads at once, no matter the mutex...
        return *this;
    }

    Common( const Common &other ) : file( other.file ), m_system( 0 ) {}
    Common() : m_system( 0 ) {}
};

template< typename _State >
struct Dve : Common< _State, dve_explicit_system_t >
{};

template< typename _State >
struct Bymoc : Common< _State, bymoc_explicit_system_t >
{};

typedef Dve< Blob > NDve;
typedef Bymoc< Blob > NBymoc;

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
        return State( st.ptr, true );
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
