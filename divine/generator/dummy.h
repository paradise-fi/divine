// -*- C++ -*- (c) 2007, 2008, 2009 Petr Rockai <me@mornfall.net>

#include <divine/stateallocator.h>
#include <divine/blob.h>

#ifndef DIVINE_GENERATOR_DUMMY_H
#define DIVINE_GENERATOR_DUMMY_H

namespace divine {
namespace generator {

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
        b.get< Content >( alloc->_slack ) = std::make_pair( 0, 0 );
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
            Content f = _from.get< Content >( alloc->_slack );
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
            ret.get< Content >( alloc->_slack ) =
                _from.get< Content >( alloc->_slack );
            if ( nth == 1 )
                ret.get< Content >( alloc->_slack ).first ++;
            if ( nth == 2 )
                ret.get< Content >( alloc->_slack ).second ++;
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

    bool isDeadlock( Node s ) { return false; }
    bool isGoal( Node s ) {
        Content f = s.get< Content >( alloc->_slack );
        return f.first == 512;
    }

    bool isAccepting( Node s ) { return false; }
    std::string showNode( Node s ) {
        Content f = s.get< Content >( alloc->_slack );
        std::stringstream stream;
        stream << "[" << f.first << ", " << f.second << "]";
        return stream.str();
    }

    void read( std::string ) {}

    Dummy() {
        alloc = new BlobAllocator();
    }

    Dummy( const Dummy & ) {
        alloc = new BlobAllocator();
    }

    Dummy &operator=( const Dummy & ) {
        if ( !alloc )
            alloc = new BlobAllocator();
        return *this;
    }
};

}
}

#endif
