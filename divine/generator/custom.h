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

struct Custom : Common {
    typedef Blob Node;
    std::string file;

    typedef void (*dl_get_initial_state_t)(char *);
    typedef size_t (*dl_get_state_size_t)();
    typedef int (*dl_get_successor_t)(int, char *, char *);
    typedef bool (*dl_is_accepting_t)(char *, int);
    typedef void (*dl_get_many_successors_t)(int, char *, char *, char *, char *);

    struct Dl {
        void *handle;
        dl_get_initial_state_t get_initial_state;
        dl_get_successor_t get_successor;
        dl_is_accepting_t is_accepting;
        dl_get_state_size_t get_state_size;
        dl_get_many_successors_t get_many_successors;
        size_t size;

        Dl() : get_initial_state( 0 ), get_successor( 0 ), is_accepting( 0 ),
               get_state_size( 0 ), get_many_successors( 0 ),
               size( 0 ) {}
    } dl;

    typedef wibble::Unit CircularSupport;

    struct Successors {
        typedef Node Type;
        Node _from;
        mutable Node my;
        mutable int handle;
        Custom *custom;

        int result() {
            return 0;
        }

        bool empty() const {
            if ( !_from.valid() )
                return true;
            force();
            return handle == 0;
        }

        Node from() { return _from; }

        void force() const {
            if ( my.valid() ) return;
            my = custom->alloc.new_blob( custom->dl.size );
            handle = custom->dl.get_successor(
                handle,
                custom->nodeData( _from ), custom->nodeData( my ) );
            if ( handle == 0 )
                custom->release( my );
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

    template< typename C1, typename C2 >
    void fillCircular( C1 &in, C2 &out )
    {
        if ( in.empty() )
            return;
#ifndef DISABLE_POOLS
        if ( dl.get_many_successors ) {
            Pool *p = &pool();
            dl.get_many_successors( alloc._slack, (char *) p,
                                    (char *) &(p->m_groups.front()),
                                    (char *) &in, (char *) &out );
        } else
#endif
            fillCircularTedious( *this, in, out );
    }

    char *nodeData( Blob b ) {
        return b.data() + alloc._slack;
    }

    int nodeSize( Blob b ) {
        return b.size() - alloc._slack;
    }

    Node initial() {
        Blob b = alloc.new_blob( dl.size );
        dl.get_initial_state( nodeData( b ) );
        return b;
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

        dl.get_initial_state = (dl_get_initial_state_t)
                               dlsym(dl.handle, "get_initial_state");
        dl.get_successor = (dl_get_successor_t)
                           dlsym(dl.handle, "get_successor");
        dl.is_accepting = (dl_is_accepting_t)
                           dlsym(dl.handle, "is_accepting");
        dl.get_state_size = (dl_get_state_size_t)
                            dlsym(dl.handle, "get_state_size");
        dl.get_many_successors = (dl_get_many_successors_t)
                                 dlsym(dl.handle, "get_many_successors");

        if( !dl.get_initial_state )
            die( "FATAL: Could not resolve get_initial_state." );
        if( !dl.get_state_size )
            die( "FATAL: Could not resolve get_state_size." );
        if( !dl.get_successor && !dl.get_many_successors )
            die( "FATAL: Could not resolve neither of get_successor/get_many_successors." );

        dl.size = dl.get_state_size();

        std::cerr << path << " loaded [state size = "
                  << dl.size << "]..." << std::endl;
    }

    bool isDeadlock( Node s ) { return false; } // XXX
    bool isGoal( Node s ) { return false; } // XXX

    bool isAccepting( Node s ) {
        if ( dl.is_accepting )
            return dl.is_accepting( nodeData( s ), nodeSize( s ) );
        else
            return false;
    }

    std::string showNode( Node s ) { return "Foo."; } // XXX

    void release( Node s ) { s.free( pool() ); }
};

}
}

#endif
