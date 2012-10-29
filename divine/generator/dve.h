// -*- C++ -*- (c) 2007, 2008, 2009 Petr Rockai <me@mornfall.net>

#ifdef O_DVE
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
    typedef typename Common::Label Label;

    template< typename Yield >
    void successors( Node from, Yield yield ) {
        if ( !from.valid() )
            return;

        updateMem( from );
        dve::System::Continuation p;

        p = system->enabled( ctx, p );

        while ( system->valid( ctx, p ) ) {
            Blob b = alloc.new_blob( stateSize() );
            memcpy( mem( b ), mem( from ), stateSize() );
            updateMem( b );
            system->apply( ctx, p );
            yield( b, Label() );
            updateMem( from );
            p = system->enabled( ctx, p );
        }
    }

    Node initial() {
        Blob b = alloc.new_blob( stateSize() );
        updateMem( b );
        system->initial( ctx );
        return b;
    }

    template< typename Q >
    void queueInitials( Q &q ) {
        q.queue( Node(), initial(), Label() );
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

    template< typename O >
    O getProperties( O o ) {
        assert( system );
        if ( system->property )
            *o++ = std::make_pair( wibble::str::fmt( system->property->id ), "" );
        for ( int i = 0; i < system->properties.size(); ++i )
            *o++ = std::make_pair( wibble::str::fmt( system->properties[i].id ), "" );
        return o;
    }

    void useProperty( meta::Input &i ) {
        if ( i.propertyName == "none" )
            system->property = NULL;
    }

    PropertyType propertyType() {
        return AC_None;
    }

    int stateSize() {
        return system->symtab.context->offset;
    }

    bool isGoal( Node s ) {
        updateMem( s );
        return !system->assertValid( ctx );
    }

    bool isAccepting( Node s ) {
        updateMem( s );
        return system->accepting( ctx );
    }

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

    /// currently only dummy method
    std::string showTransition( Node from, Node to ) {
        return "";
    }

    void release( Node s ) { s.free( pool() ); }
    Dve() : system( 0 ) {}
};

}
}

#endif
#endif
