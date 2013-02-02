// -*- C++ -*- (c) 2007, 2008, 2009 Petr Rockai <me@mornfall.net>

#include <divine/generator/common.h>

#ifdef POSIX
#include <dlfcn.h>
#elif defined _WIN32
#include <external/dlfcn-win32/dlfcn.h>
#endif

#ifndef DIVINE_GENERATOR_CESMI_H
#define DIVINE_GENERATOR_CESMI_H

/*
 * Add alloc function to setup, to make the interface more opaque.
 */

namespace divine {
namespace generator {

static const char * const cesmi_ext =
#if defined(_WIN32)
    ".dll";
#else
    ".so";
#endif

namespace cesmi {
#include <divine/cesmi/usr-cesmi.h>
};

/**
 * A binary model loader. The model is expected to be a shared object (i.e. .so
 * on ELF systems, and a .dll on win32) and conform to the CESMI specification.
 */

struct CESMI : public Common< Blob > {
    typedef Blob Node;
    typedef generator::Common< Blob > Common;
    typedef typename Common::Label Label;
    std::string file;

    struct Dl {
        void *handle;

        decltype( &cesmi::setup             ) setup;
        decltype( &cesmi::get_property_type ) get_property_type;
        decltype( &cesmi::show_property     ) show_property;

        decltype( &cesmi::get_initial       ) get_initial;
        decltype( &cesmi::get_successor     ) get_successor;
        decltype( &cesmi::get_flags         ) get_flags;

        decltype( &cesmi::show_node         ) show_node;
        decltype( &cesmi::show_transition   ) show_transition;

        Dl() :
            setup( nullptr ),
            get_property_type( nullptr ),
            show_property( nullptr ),
            get_initial( nullptr ),
            get_successor( nullptr ),
            get_flags( nullptr ),
            show_node( nullptr ),
            show_transition( nullptr )
        {}
    } dl;

    cesmi::cesmi_setup setup;

    cesmi::cesmi_node data( Blob b ) {
        cesmi::cesmi_node n = { .handle = b.ptr,
                                .memory = b.data() + alloc._slack };
        return n;
    }

    template< typename Yield, typename Get >
    void _successors( Node s, Yield yield, Get get )
    {
        int handle = 1, old = 1;
        call_setup();
        while ( handle ) {
            cesmi::cesmi_node state;
            handle = get( &setup, handle, data( s ), &state );
            if ( handle )
                yield( Blob( state.handle ), old );
            old = handle;
        }
    }

    template< typename Yield >
    void successors( Node s, Yield yield ) {
        if ( !s.valid() )
            return;
        _successors( s, [yield]( Node n, int ) { yield( n, Label() ); }, dl.get_successor );
    }

    template< typename Yield >
    void initials( Yield yield )
    {
        _successors( Node(), [yield]( Node n, int ) { yield( Node(), n, Label() ); },
                     [this]( cesmi::cesmi_setup *s, int h,
                             cesmi::cesmi_node, cesmi::cesmi_node *to ) -> int
                     {
                         return this->dl.get_initial( s, h, to );
                     } );
    }

    void die( const char *fmt, ... ) __attribute__((noreturn)) {
        va_list ap;
        va_start( ap, fmt );
        vfprintf( stderr, fmt, ap );
        exit( 1 );
    }

    template< typename T >
    void getsym( T *result, const char *name ) {
        static_assert( sizeof( T ) == sizeof( void * ), "pointer size mismatch" );
        union { T sym; void *ptr; };
        ptr = dlsym( dl.handle, name );
        *result = sym;
    }

    void read( std::string path ) {
        // dlopen does not like path-less shared objects outside lib locations
        if ( wibble::str::basename( path ) == path )
            path = "./" + path;

        dl.handle = dlopen( path.c_str(), RTLD_LAZY );

        if( !dl.handle )
            die( "FATAL: Error loading \"%s\".\n%s", path.c_str(), dlerror() );

        getsym( &dl.setup, "setup" );
        getsym( &dl.get_property_type, "get_property_type" );
        getsym( &dl.show_property, "show_property" );

        getsym( &dl.get_initial, "get_initial" );
        getsym( &dl.get_successor, "get_successor" );
        getsym( &dl.get_flags , "get_flags" );

        getsym( &dl.show_node, "show_node" );
        getsym( &dl.show_transition, "show_transition" );

        if ( !dl.get_initial )
            die( "FATAL: Could not resolve get_initial." );
        if ( !dl.get_successor )
            die( "FATAL: Could not resolve get_successor." );
    }

    static cesmi::cesmi_node make_node( void *handle, int size ) {
        CESMI *_this = reinterpret_cast< CESMI * >( handle );
        int slack = _this->alloc._slack;
        Blob b( _this->alloc.pool(), size + slack );
        b.clear( 0, slack );
        cesmi::cesmi_node n;
        n.memory = b.data() + slack;
        n.handle = b.ptr;
        return n;
    }

    static cesmi::cesmi_node clone_node( void *handle, cesmi::cesmi_node orig ) {
        CESMI *_this = reinterpret_cast< CESMI * >( handle );
        Blob origb( orig.handle );
        Blob b( _this->alloc.pool(), origb.size() );

        int slack = _this->alloc._slack;
        origb.copyTo( b );
        b.clear( 0, slack );
        cesmi::cesmi_node n;
        n.memory = b.data() + slack;
        n.handle = b.ptr;
        return n;
    }

    void call_setup()
    {
        if ( setup.instance_initialised )
            return;

        setup.property = 0;
        setup.property_count = 0;
        setup.allocation_handle = this;
        setup.make_node = &make_node;
        setup.clone_node = &clone_node;
        setup.instance = 0;
        setup.instance_initialised = 0;

        if ( dl.setup )
            dl.setup( &setup );

        setup.instance_initialised = 1;
    }

    std::string propertyName( int i ) {
        char *descr = dl.show_property ? dl.show_property( &setup, i ) : nullptr;
        std::string r = descr ? std::string( descr ) : ("p_" + wibble::str::fmt( i + 1 ));
        ::free( descr );
        return r;
    }

    template< typename Y >
    void properties( Y yield ) {
        call_setup();
        for ( int i = 0; i < setup.property_count; ++i ) {
            auto name = propertyName( i );
            const char *type = "unknown";
            switch (dl.get_property_type ? dl.get_property_type( &setup, i ) : cesmi::cesmi_pt_deadlock) {
                case cesmi::cesmi_pt_deadlock:
                    yield( name, "deadlock freedom", PT_Deadlock );
                    continue;
                case cesmi::cesmi_pt_goal:
                    yield( name, "safety", PT_Goal );
                    continue;
                case cesmi::cesmi_pt_buchi:
                    yield( name, "neverclaim / LTL verification", PT_Buchi );
                    continue;
            }
        }
    }

    void useProperty( std::string n ) {
        call_setup();
        for ( int i = 0; i < setup.property_count; ++i ) {
            if ( n == propertyName( i ) ) {
                setup.property = i;
                return;
            }
        }
        throw wibble::exception::Consistency( "Unknown property " + n + ". Please consult divine info." );
    }

    CESMI() {
        setup.instance_initialised = 0;
    }

    CESMI( const CESMI &other ) {
        dl = other.dl;
        setup.instance_initialised = 0;
    }

    uint64_t flags( Node s ) {
        if ( dl.get_flags )
            return dl.get_flags( &setup, data( s ) );
        else
            return 0;
    }

    bool isGoal( Node s ) { return flags( s ) & cesmi::cesmi_goal; }
    bool isAccepting( Node s ) { return flags( s ) & cesmi::cesmi_accepting; }

    std::string showNode( Node s ) {
        if ( dl.show_node ) {
            char *fmt = dl.show_node( &setup, data( s ) );
            std::string s( fmt );
            ::free( fmt );
            return s;
        } else
            return "()";
    }

    std::string showTransition( Node from, Node to, Label ) {
        char *fmt = nullptr;

        if ( dl.show_transition && from.valid() ) {
            _successors( from, [&]( Node n, int handle ) {
                    if ( to.compare( n, alloc._slack, n.size() ) == 0 )
                        fmt = dl.show_transition( &setup, data( from ), handle );
                }, dl.get_successor );
        }

        if ( fmt ) {
            std::string s( fmt );
            ::free( fmt );
            return s;
        } else
            return "";
    }

    Node initial() { assert_die(); }
    void release( Node s ) { s.free( pool() ); }
};

}
}

#endif
