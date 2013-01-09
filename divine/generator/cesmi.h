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

        Dl() : get_initial( 0 ), get_successor( 0 ), get_flags( 0 ), show_node( 0 ),
               show_transition( 0 ), setup( 0 ), get_property_type( 0 ), show_property( 0 ) {}
    } dl;

    cesmi::cesmi_setup setup;

    char *data( Blob b ) {
        return b.data() + alloc._slack;
    }

    template< typename Yield >
    void _successors( Node s, Yield yield ) {
        int handle = 1;
        if ( !s.valid() )
            return;

        call_setup();

        while ( handle ) {
            cesmi::cesmi_node state;
            handle = dl.get_successor( &setup, handle, data( s ), &state );
            if ( handle )
                yield( Blob( state.handle ), handle );
        }
    }

    template< typename Yield >
    void successors( Node s, Yield yield ) {
        _successors( s, [yield]( Node n, int ) { yield( n, Label() ); } );
    }

    template< typename Q >
    void queueInitials( Q &q ) {
        int handle = 1;
        call_setup();
        while ( handle ) {
            cesmi::cesmi_node state;
            handle = dl.get_initial( &setup, handle, &state );
            if ( handle )
                q.queue( Node(), Blob( state.handle ), Label() );
        } while ( handle );
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

#ifndef O_POOLS
        die( "FATAL: Pool support is required for the CESMI generator." );
#endif
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

    void call_setup()
    {
        if ( setup.instance_initialised )
            return;

        setup.property_count = 0;
        setup.allocation_handle = this;
        setup.make_node = &make_node;
        setup.instance = 0;
        setup.instance_initialised = 0;

        if ( dl.setup )
            dl.setup( &setup );

        setup.instance_initialised = 1;
    }

    /* XXX this interface is wrong */
    PropertyType propertyType() {
        call_setup();
        switch ( dl.get_property_type( &setup, 0 ) ) {
            case cesmi::cesmi_pt_buchi: return AC_Buchi;
            case cesmi::cesmi_pt_goal: return AC_None;
        }
        return AC_None; /* Duh. */
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

        if ( dl.show_transition ) {
            _successors( from, [&]( Node n, int handle ) {
                    if ( to.compare( n, alloc._slack, 0 ) == 0 )
                        fmt = dl.show_transition( &setup, data( from ), handle );
                } );
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
