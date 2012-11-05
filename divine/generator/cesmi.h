// -*- C++ -*- (c) 2007, 2008, 2009 Petr Rockai <me@mornfall.net>

#include <divine/generator/common.h>

#ifdef POSIX
#include <dlfcn.h>
#elif defined _WIN32
#include <external/dlfcn-win32/dlfcn.h>
#endif

#ifndef DIVINE_GENERATOR_CESMI_H
#define DIVINE_GENERATOR_CESMI_H

#include <divine/generator/cesmi-client.h>

namespace divine {
namespace generator {

/**
 * A binary model loader. The model is expected to be a shared object (i.e. .so
 * on ELF systems, and a .dll on win32) and conform to the CESMI specification.
 */

struct CESMI : public Common< Blob > {
    typedef Blob Node;
    typedef generator::Common< Blob > Common;
    typedef typename Common::Label Label;
    std::string file;

    typedef void (*dl_setup_t)(CESMISetup *);
    typedef void (*dl_get_initial_t)(CESMISetup *, char **);
    typedef int (*dl_get_successor_t)(CESMISetup *, int, char *, char **);
    typedef bool (*dl_is_accepting_t)(CESMISetup *, char *);
    typedef char *(*dl_show_node_t)(CESMISetup *, char *);
    typedef char *(*dl_show_transition_t)(CESMISetup *, char *, char *);
    typedef void (*dl_cache_successors_t)(CESMISetup *, SuccessorCache *);

    struct Dl {
        void *handle;
        dl_get_initial_t get_initial;
        dl_get_successor_t get_successor;
        dl_is_accepting_t is_accepting;
        dl_show_node_t show_node;
        dl_show_transition_t show_transition;
        dl_setup_t setup;
        dl_cache_successors_t cache_successors;

        Dl() : get_initial( 0 ), get_successor( 0 ), is_accepting( 0 ), show_node( 0 ),
               show_transition( 0 ), setup( 0 ), cache_successors( 0 ) {}
    } dl;

    CESMISetup setup;

    template< typename Yield >
    void successors( Node s, Yield yield ) {
        int handle = 1;
        if ( !s.valid() )
            return;

        while ( handle ) {
            char *state;
            handle = dl.get_successor( &setup, handle, s.pointer(), &state );
            if ( handle )
                yield( Blob( state ), Label() );
        }
    }

    Node initial() {
        char *state;
        assert_eq( setup.pool, &pool() );
        dl.get_initial( &setup, &state );
        return Blob( state );
    }

    template< typename Q >
    void queueInitials( Q &q ) {
        q.queue( Node(), initial(), Label() );
    }

    void die( const char *fmt, ... ) __attribute__((noreturn)) {
        va_list ap;
        va_start( ap, fmt );
        vfprintf( stderr, fmt, ap );
        exit( 1 );
    }

    void read( std::string path ) {
        // dlopen does not like path-less shared objects outside lib locations
        if ( wibble::str::basename( path ) == path )
            path = "./" + path;

        dl.handle = dlopen( path.c_str(), RTLD_LAZY );

        if( !dl.handle )
            die( "FATAL: Error loading \"%s\".\n%s", path.c_str(), dlerror() );

        dl.get_initial = (dl_get_initial_t) dlsym(dl.handle, "get_initial");
        dl.setup = (dl_setup_t) dlsym(dl.handle, "setup");
        dl.get_successor = (dl_get_successor_t) dlsym(dl.handle, "get_successor");
        dl.is_accepting = (dl_is_accepting_t) dlsym(dl.handle, "is_accepting");
        dl.show_node = (dl_show_node_t) dlsym(dl.handle, "show_node");
        dl.cache_successors = (dl_cache_successors_t) dlsym(dl.handle, "cache_successors");

        if( !dl.get_initial )
            die( "FATAL: Could not resolve get_initial." );
        if( !dl.get_successor )
            die( "FATAL: Could not resolve get_successor." );

#ifndef O_POOLS
        die( "FATAL: Pool support is required for the CESMI generator." );
#endif

        call_setup();
    }

    void call_setup()
    {
        setup.has_property = 0;
        setup.slack = alloc._slack;
        setup.pool = &pool();
        if ( dl.setup )
            dl.setup( &setup );
    }

    PropertyType propertyType() {
        return setup.has_property ? AC_Buchi : AC_None;
    }

    CESMI() {}
    CESMI( const CESMI &other ) {
        dl = other.dl;
        setup = other.setup;
        call_setup();
    }


    bool isGoal( Node s ) { return false; } // XXX

    bool isAccepting( Node s ) {
        if ( dl.is_accepting )
            return dl.is_accepting( &setup, s.pointer() );
        else
            return false;
    }

    std::string showNode( Node s ) {
        if ( dl.show_node ) {
            char *fmt = dl.show_node( &setup, s.pointer() );
            std::string s( fmt );
            ::free( fmt );
            return s;
        } else
            return "()";
    }

    std::string showTransition( Node from, Node to, Label ) {
        if ( dl.show_transition ) {
            char *fmt = dl.show_transition( &setup, from.pointer(), to.pointer() );
            std::string s( fmt );
            ::free( fmt );
            return s;
        } else
            return "";
    }

    void release( Node s ) { s.free( pool() ); }
};

}
}

#endif
