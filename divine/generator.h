// -*- C++ -*- (c) 2007 Petr Rockai <me@mornfall.net>

#include <wibble/sys/mutex.h>

#include <divine/legacy/system/dve/dve_explicit_system.hh>
#include <divine/legacy/system/bymoc/bymoc_explicit_system.hh>

#include <divine/stateallocator.h>
#include <divine/blob.h>
#include <dlfcn.h>

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
            if ( !_from.valid() )
                return true;
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
        wibble::sys::MutexLock __l( readMutex() );
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
            wibble::sys::MutexLock __l( readMutex() );
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

struct Dummy {
    typedef std::pair< short, short > Content;
    typedef Blob Node;
    BlobAllocator *alloc;

    void setAllocator( StateAllocator *a ) {
        alloc = dynamic_cast< BlobAllocator * >( a );
        assert( alloc );
    }

    Node initial() {
        Blob b = alloc->new_blob( sizeof( Content ) );
        b.get< Content >() = std::make_pair( 0, 0 );
        return b;
    }

    struct Successors {
        divine::BlobAllocator *alloc;
        Node _from;
        int nth;

        int result() {
            return 0;
        }

        bool empty() {
            if ( !_from.valid() )
                return true;
            Content f = _from.get< Content >();
            if ( f.first == 1024 || f.second == 1024 )
                return true;
            return nth == 3;
        }

        Node from() { return _from; }

        Successors tail() {
            Successors s = *this;
            s.nth ++;
            return s;
        }

        Node head() {
            Node ret = alloc->new_blob( sizeof( Content ) );
            ret.get< Content >() = _from.get< Content >();
            if ( nth == 1 )
                ret.get< Content >().first ++;
            if ( nth == 2 )
                ret.get< Content >().second ++;
            return ret;
        }
    };

    int stateSize() {
        return sizeof( Content );
    }

    Successors successors( Node st ) {
        Successors ret;
        ret.alloc = alloc;
        ret._from = st;
        ret.nth = 1;
        return ret;
    }

    void release( Node s ) {
        s.free( alloc->alloc() );
    }

    bool is_accepting( Node s ) {
        return false;
    }

    void read( std::string ) {}

    Dummy() {
        alloc = new BlobAllocator( 0 );
    }
};

struct Custom {
    typedef Blob Node;
    std::string file;
    void *dl;
    BlobAllocator *alloc;
    size_t size;

    typedef void (*dl_get_initial_state_t)(char *);
    typedef size_t (*dl_get_state_size_t)();
    typedef int (*dl_get_successor_t)(int, char *, char *);

    dl_get_initial_state_t dl_get_initial_state;
    dl_get_successor_t dl_get_successor;
    dl_get_state_size_t dl_get_state_size;

    struct Successors {
        Node _from, my;
        int handle;
        Custom *custom;

        int result() {
            return 0;
        }

        bool empty() {
            if ( !_from.valid() )
                return true;
            force();
            return handle == 0;
        }

        Node from() { return _from; }

        void force() {
            if ( my.valid() ) return;
            my = custom->alloc->new_blob( custom->size );
            handle = custom->dl_get_successor(
                handle, _from.data(), my.data() );
        }

        Node head() {
            force();
            return my;
        }

        Successors tail() {
            force();
            Successors s = *this;
            s.my = Blob();
            return s;
        }
    };

    Successors successors( Node s ) {
        Successors succ;
        succ._from = s;
        succ.custom = this;
        succ.handle = 1;
        return succ;
    }

    Node initial() {
        Blob b = alloc->new_blob( size );
        dl_get_initial_state( b.data() );
        return b;
    }

    void setAllocator( StateAllocator *a ) {
        alloc = dynamic_cast< BlobAllocator * >( a );
        assert( alloc );
    }

    int stateSize() {
        return size;
    }

    void read( std::string path ) {
        dl = dlopen(path.c_str(), RTLD_LAZY);

        assert( dl );

        dl_get_initial_state = (dl_get_initial_state_t) dlsym(dl, "get_initial_state");
        dl_get_successor = (dl_get_successor_t) dlsym(dl, "get_successor");
        dl_get_state_size = (dl_get_state_size_t) dlsym(dl, "get_state_size");

        assert( dl_get_initial_state );
        assert( dl_get_successor );
        assert( dl_get_state_size );

        size = dl_get_state_size();
        
        std::cerr << path << " loaded [size = " << size << "]..." << std::endl;
    }

    bool is_accepting( Node s ) {
        return false;
    }

    void release( Node s ) {
        s.free( alloc->alloc() );
    }

    Custom() : dl( 0 ),
               dl_get_initial_state( 0 ),
               dl_get_successor( 0 ),
               dl_get_state_size( 0 ) {
        alloc = new BlobAllocator( 0 );
    }
};

}
}

#endif
