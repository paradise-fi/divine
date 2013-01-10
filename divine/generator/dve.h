// -*- C++ -*- (c) 2007, 2008, 2009 Petr Rockai <me@mornfall.net>

#ifdef O_DVE
#include <divine/generator/common.h>

#ifndef DIVINE_GENERATOR_DVE_H
#define DIVINE_GENERATOR_DVE_H

#include <deque>
#include <vector>
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
    void enabledConts( Node from, Yield yield ) {
        if ( !from.valid() )
            return;

        updateMem( from );
        dve::System::Continuation p;

        p = system->enabled( ctx, p );

        while ( system->valid( ctx, p ) ) {
            if ( !yield( p ) )
                return;
            updateMem( from );
            p = system->enabled( ctx, p );
        }
    }

    template< typename Yield >
    void processConts( Node from, Yield yield, int pid, bool include ) {
        enabledConts(
            from,
            [&]( dve::System::Continuation p ) {
                if ( this->system->processAffected( this->ctx, p, pid ) != include )
                    return true;
                if ( !yield( p ) )
                    return false;
                return true;
            }
        );
    }

    template< typename Yield >
    void yieldSuccessor( Node from, dve::System::Continuation p, Yield yield) {
        Blob b = alloc.new_blob( stateSize() );
        memcpy( mem( b ), mem( from ), stateSize() );
        updateMem( b );
        system->apply( ctx, p );
        yield( b );
    }

    template< typename Yield >
    void successors( Node from, Yield yield ) {
        enabledConts(
            from,
            [&]( dve::System::Continuation p ) {
                yieldSuccessor( from, p, [&]( Node n ) { yield( n, Label() ); } );
                return true;
            }
        );
    }

    bool isDiamond( Node from, size_t process,
                    std::deque< dve::System::Continuation > ampleSet,
                    std::deque< dve::System::Continuation > indepSet ) 
    {
        std::deque< Node > ampleNodes, indepNodes;
        std::vector< std::deque< Node > > ampleNodesSuccs;
        ampleNodesSuccs.resize( ampleSet.size() );
        bool ok = true;
        for ( auto i: ampleSet ) {
            yieldSuccessor( from, i, [&]( Node n ) { ampleNodes.push_back( n ); } );
            auto indepIt = indepSet.cbegin();
            processConts(
                ampleNodes.back(),
                [&]( dve::System::Continuation p ) {
                    if ( indepIt == indepSet.cend() || *indepIt != p ) {
                        ok = false;
                        return false;
                    }
                    yieldSuccessor(
                        ampleNodes.back(),
                        p,
                        [&]( Node n ) { 
                            ampleNodesSuccs[ ampleNodes.size() - 1 ].push_back( n );
                        }
                    );
                    indepIt++;
                    return true;
                },
                process,
                false
            );
            if ( !ok || indepIt != indepSet.cend() )
                return false;
        }
        for ( auto i: indepSet ) {
            yieldSuccessor( from, i, [&]( Node n ) { indepNodes.push_back( n ); } );
            auto ampleIt = ampleSet.cbegin();
            auto ampleNSIt = ampleNodesSuccs.begin();
            processConts(
                indepNodes.back(),
                [&]( dve::System::Continuation p ) {
                    if ( ampleIt == ampleSet.cend() || *ampleIt != p ) {
                        ok = false;
                        return false;
                    }
                    assert( ampleNSIt != ampleNodesSuccs.end() );
                    assert( !ampleNSIt->empty() );
                    yieldSuccessor(
                        indepNodes.back(),
                        p,
                        [&]( Node n ) {
                            if ( n.compare( ampleNSIt->front(), this->alloc._slack, n.size() ) != 0 )
                                ok = false;
                            ampleNSIt->pop_front();
                        }
                    );
                    if ( !ok )
                        return false;
                    ampleIt++;
                    ampleNSIt++;
                    return true;
                },
                process,
                true
            );
            if ( !ok || ampleIt != ampleSet.cend() )
                return false;
        }
        return true;
    }

    template< typename Yield >
    void ample( Node from, Yield yield ) {
        std::deque< dve::System::Continuation > ampleCands, indep;
        for ( int i = 0; i < system->processCount(); i++ ) {
            bool tryNext = false;
            processConts(
                from,
                [&]( dve::System::Continuation p ) {
                    dve::Transition &trans = this->system->getTransition( this->ctx, p );
                    if ( trans.sync ) {
                        tryNext = true;
                        return false;
                    }
                    ampleCands.push_back( p );
                    return true;
                },
                i,
                true
            );

            if ( tryNext || ampleCands.empty() ) {
                ampleCands.clear();
                continue;
            }

            processConts(
                from,
                [&]( dve::System::Continuation p ) {
                    indep.push_back( p );
                    return true;
                },
                i,
                false
            );
            if ( !isDiamond( from, i, ampleCands, indep ) ) {
                ampleCands.clear();
                indep.clear();
                continue;
            }
            while ( !ampleCands.empty() ) {
                dve::System::Continuation p = ampleCands.front();
                yieldSuccessor( from, p, [&]( Node n ) { yield( n, Label() ); } );
                ampleCands.pop_front();
            }
            return;
        }
        successors( from, yield );
    }

    template< typename Yield >
    void processSuccessors( Node from, Yield yield, int pid, bool include ) {
        processConts(
            from,
            [&]( dve::System::Continuation p ) {
                yieldSuccessor( from, p, [&]( Node n ) { yield( n, Label() ); } );
                return true;
            },
            pid,
            include
        );
    }

    int processCount() {
        return system->processCount();
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
        return system->property ? AC_Buchi : AC_None;
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

    std::string showTransition( Node from, Node to, Label ) {
        if ( !from.valid() || !to.valid() )
            return "";

        std::string transLabel = "";

        enabledConts( from, [&]( dve::System::Continuation p ) {
                Blob b = this->alloc.new_blob( stateSize() );
                memcpy( mem( b ), mem( from ), stateSize() );
                updateMem( b );

                this->system->apply( this->ctx, p );

                updateMem( from );
                if ( b.compare( to, this->alloc._slack, b.size() ) == 0 ) {
                    std::stringstream str;
                    this->system->printTrans( str, this->ctx, p );
                    transLabel = str.str();
                    return false;
                }
                return true;
            }
        );

        return transLabel;
    }

    void release( Node s ) { s.free( pool() ); }
    Dve() : system( 0 ) {}
};

}
}

#endif
#endif
