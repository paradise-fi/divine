// -*- C++ -*- (c) 2007, 2008, 2009 Petr Rockai <me@mornfall.net>

#include <divine/generator/common.h>

#ifndef DIVINE_GENERATOR_DVE_H
#define DIVINE_GENERATOR_DVE_H

#include <divine/dve/interpreter.h>

namespace divine {
namespace generator {

struct Dve : public Common< Blob > {
    struct dve::System *system;
    struct dve::EvalContext ctx;
    typedef Blob Node;
    typedef generator::Common< Blob > Common;

    struct Successors {
        typedef Node Type;

        Node _from;
        mutable int idx;
        std::vector< dve::Transition > enabled;
        Dve* parent;

        bool empty() const {
            return idx >= enabled.size();
        }

        Node from() { return _from; }

        Node head() {
            assert( idx < enabled.size() );
            Blob b = parent->alloc.new_blob( 100 ); // XXX
            memcpy( parent->mem( b ), parent->mem( _from ), 100 );
            parent->updateMem( b );
            enabled[ idx ].apply( parent->ctx );
            return b;
        }

        Successors tail() {
            Successors s = *this;
            s.idx = idx + 1;
            return s;
        }
    };

    Successors successors( Node s ) {
        Successors succ;
        succ._from = s;
        updateMem( s );
        system->enabled( ctx, std::back_inserter( succ.enabled ) );
        std::cerr << showNode( s ) << " has " << succ.enabled.size() << " successors" << std::endl;
        succ.parent = this;
        succ.idx = 0;
        return succ;
    }

    Node initial() {
        Blob b = alloc.new_blob( 100 ); // XXX
        return b;
    }

    template< typename Q >
    void queueInitials( Q &q ) {
        q.queue( Node(), initial() );
    }

    void read( std::string path ) {
        std::ifstream file;
        file.open( path.c_str() );
        dve::IOStream stream( file );
        dve::Lexer< dve::IOStream > lexer( stream );
        dve::Parser::Context ctx( lexer );
        try {
            dve::parse::System ast( ctx );
            system = new dve::System( ast );
        } catch (...) {
            ctx.errors( std::cerr );
            throw;
        }
    }

    PropertyType propertyType() {
        return AC_None;
    }

    // XXX
    bool isGoal( Node s ) { return false; }
    bool isAccepting( Node s ) { return false; }

    char *mem( Node s ) {
        return &s.get< char >( alloc._slack );
    }

    void updateMem( Node s ) {
        ctx.mem = mem( s );
    }

    std::string showNode( Node s ) {
        std::stringstream str;
        system->symtab.dump( str, mem( s ) );
        return str.str();
    }

    void release( Node s ) { s.free( pool() ); }
};

}
}

#endif
