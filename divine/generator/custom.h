// -*- C++ -*- (c) 2007, 2008, 2009 Petr Rockai <me@mornfall.net>

#include <divine/generator/common.h>

#include <dlfcn.h>

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
    typedef void (*dl_get_many_successors_t)(char *, char *, char *, char *);

    struct Dl {
        void *handle;
        dl_get_initial_state_t get_initial_state;
        dl_get_successor_t get_successor;
        dl_get_state_size_t get_state_size;
        dl_get_many_successors_t get_many_successors;
        size_t size;

        Dl() : get_initial_state( 0 ), get_successor( 0 ),
               get_state_size( 0 ), get_many_successors( 0 ),
               size( 0 ) {}
    } dl;

    typedef wibble::Unit CircularSupport;

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
            my = custom->alloc.new_blob( custom->dl.size );
            handle = custom->dl.get_successor(
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

    template< typename C1, typename C2 >
    void fillCircular( C1 &in, C2 &out )
    {
        if ( in.empty() )
            return;
#ifndef DISABLE_POOLS
        if ( dl.get_many_successors ) {
            Pool *p = &pool();
            dl.get_many_successors( (char *) p, (char *) &(p->m_groups.front()),
                                    (char *) &in, (char *) &out );
        } else
#endif
            fillCircularTedious( *this, in, out );
    }

    Node initial() {
        Blob b = alloc.new_blob( dl.size );
        dl.get_initial_state( b.data() );
        return b;
    }

    void read( std::string path ) {
        dl.handle = dlopen( path.c_str(), RTLD_LAZY );

        assert( dl.handle );

        dl.get_initial_state = (dl_get_initial_state_t)
                               dlsym(dl.handle, "get_initial_state");
        dl.get_successor = (dl_get_successor_t)
                           dlsym(dl.handle, "get_successor");
        dl.get_state_size = (dl_get_state_size_t)
                            dlsym(dl.handle, "get_state_size");
        dl.get_many_successors = (dl_get_many_successors_t)
                                 dlsym(dl.handle, "get_many_successors");

        assert( dl.get_initial_state );
        assert( dl.get_state_size );
        assert( dl.get_successor || dl.get_many_successors );

        dl.size = dl.get_state_size();

        std::cerr << path << " loaded [state size = "
                  << dl.size << "]..." << std::endl;
    }

    bool isDeadlock( Node s ) { return false; } // XXX
    bool isGoal( Node s ) { return false; } // XXX
    bool isAccepting( Node s ) { return false; } // XXX
    std::string showNode( Node s ) { return "Foo."; } // XXX

    void release( Node s ) { s.free( pool() ); }
};

}
}

#endif
