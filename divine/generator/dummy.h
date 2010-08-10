// -*- C++ -*- (c) 2007, 2008, 2009 Petr Rockai <me@mornfall.net>

#include <divine/generator/common.h>

#ifndef DIVINE_GENERATOR_DUMMY_H
#define DIVINE_GENERATOR_DUMMY_H

namespace divine {
namespace generator {

struct Dummy : Common {
    typedef std::pair< short, short > Content;
    typedef Blob Node;

    Node initial() {
        Blob b = alloc.new_blob( sizeof( Content ) );
        b.get< Content >( alloc._slack ) = std::make_pair( 0, 0 );
        return b;
    }

    template< typename Q >
    void queueInitials( Q &q ) {
        q.queue( Node(), initial() );
    }

    struct Successors {
        typedef Node Type;
        mutable Node _from;
        int nth;
        Dummy *parent;

        bool empty() const {
            if ( !_from.valid() )
                return true;
            Content f = _from.get< Content >( parent->alloc._slack );
            if ( f.first == 1024 || f.second == 1024 )
                return true;
            return nth == 3;
        }

        Node from() { return _from; }

        Successors tail() const {
            Successors s = *this;
            s.nth ++;
            return s;
        }

        Node head() {
            Node ret = parent->alloc.new_blob( sizeof( Content ) );
            ret.get< Content >( parent->alloc._slack ) =
                _from.get< Content >( parent->alloc._slack );
            if ( nth == 1 )
                ret.get< Content >( parent->alloc._slack ).first ++;
            if ( nth == 2 )
                ret.get< Content >( parent->alloc._slack ).second ++;
            return ret;
        }
    };

    Successors successors( Node st ) {
        Successors ret;
        ret._from = st;
        ret.nth = 1;
        ret.parent = this;
        return ret;
    }

    void release( Node s ) {
        s.free( pool() );
    }

    bool isGoal( Node s ) {
        Content f = s.get< Content >( alloc._slack );
        return f.first == 512;
    }

    bool isAccepting( Node s ) { return false; }
    std::string showNode( Node s ) {
        if ( !s.valid() )
            return "[]";
        Content f = s.get< Content >( alloc._slack );
        std::stringstream stream;
        stream << "[" << f.first << ", " << f.second << "]";
        return stream.str();
    }

    void read( std::string ) {}
};

}
}

#endif
