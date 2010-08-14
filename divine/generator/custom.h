// -*- C++ -*- (c) 2007, 2008, 2009 Petr Rockai <me@mornfall.net>

#include <divine/generator/common.h>

#ifdef POSIX
#include <dlfcn.h>
#elif defined _WIN32
#include <divine/dlfcn-win32.h>
#endif

#ifndef DIVINE_GENERATOR_CUSTOM_H
#define DIVINE_GENERATOR_CUSTOM_H

namespace divine {
namespace generator {

#include <divine/generator/custom-api.h>

/**
 * A binary model loader. The model is expected to be a shared object (i.e. .so
 * on ELF systems, and a .dll on win32).
 */

struct Custom : Common {
    typedef Blob Node;
    std::string file;

    typedef void (*dl_setup_t)(CustomSetup *);
    typedef void (*dl_get_initial_t)(CustomSetup *, char **);
    typedef int (*dl_get_successor_t)(CustomSetup *, int, char *, char **);
    typedef bool (*dl_is_accepting_t)(CustomSetup *, char *, int);
    typedef void (*dl_cache_successors_t)(CustomSetup *, SuccessorCache *);

    struct Dl {
        void *handle;
        dl_get_initial_t get_initial;
        dl_get_successor_t get_successor;
        dl_is_accepting_t is_accepting;
        dl_setup_t setup;
        dl_cache_successors_t cache_successors;

        Dl() : get_initial( 0 ), get_successor( 0 ), is_accepting( 0 ),
               setup( 0 ), cache_successors( 0 ) {}
    } dl;

    struct Successors {
        typedef Node Type;
        Node _from;
        mutable Node my;
        mutable int handle;
        Custom *parent;

        bool empty() const {
            if ( !_from.valid() )
                return true;
            force();
            return handle == 0;
        }

        Node from() { return _from; }

        void force() const {
            if ( my.valid() || !handle ) return;
            char *state;
            handle = parent->dl.get_successor( &(parent->setup), handle, _from.pointer(), &state );
            if ( handle )
                my = Blob( state );
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

    CustomSetup setup;

    Successors successors( Node s ) {
        Successors succ;
        succ._from = s;
        succ.parent = this;
        succ.handle = 1;
        return succ;
    }

    int nodeSize( Blob b ) {
        return b.size() - alloc._slack;
    }

    Node initial() {
        char *state;
        assert_eq( setup.pool, &pool() );
        dl.get_initial( &setup, &state );
        return Blob( state );
    }

    template< typename Q >
    void queueInitials( Q &q ) {
        q.queue( Node(), initial() );
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
        dl.cache_successors = (dl_cache_successors_t) dlsym(dl.handle, "cache_successors");

        if( !dl.get_initial )
            die( "FATAL: Could not resolve get_initial." );
        if( !dl.get_successor )
            die( "FATAL: Could not resolve get_successor." );

#ifdef DISABLE_POOLS
        die( "FATAL: Pool support is required for the custom generator." );
#endif

        setup.has_property = 0;
        setup.slack = alloc._slack;
        setup.pool = &pool();
        if ( dl.setup )
            dl.setup( &setup );
    }

    bool hasProperty() {
        return setup.has_property;
    }

    Custom &operator=( const Custom &other ) {
        Common::operator=( other );
        dl = other.dl;
        setup = other.setup;

        setup.pool = &pool();
        if ( dl.setup )
            dl.setup( &setup );

        return *this;
    }


    bool isGoal( Node s ) { return false; } // XXX

    bool isAccepting( Node s ) {
        if ( dl.is_accepting )
            return dl.is_accepting( &setup, s.pointer(), nodeSize( s ) );
        else
            return false;
    }

    std::string showNode( Node s ) { return "Foo."; } // XXX

    void release( Node s ) { s.free( pool() ); }
};

}
}

#endif
