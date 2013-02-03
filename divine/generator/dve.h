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
        Node currentNode;
        std::vector< std::deque< Node > > ampleNodesSuccs;
        ampleNodesSuccs.resize( ampleSet.size() );
        bool ok = true;
        int index = 0;
        for ( auto i: ampleSet ) {
            yieldSuccessor( from, i, [&]( Node n ) { currentNode = n; } );
            auto indepIt = indepSet.cbegin();
            processConts(
                currentNode,
                [&]( dve::System::Continuation p ) {
                    if ( indepIt == indepSet.cend() || *indepIt != p ) {
                        ok = false;
                        return false;
                    }
                    yieldSuccessor(
                        currentNode,
                        p,
                        [&]( Node n ) {
                            ampleNodesSuccs[ index ].push_back( n );
                        }
                    );
                    indepIt++;
                    return true;
                },
                process,
                false
            );
            currentNode.free( alloc._pool );
            if ( !ok || indepIt != indepSet.cend() ) {
                for ( auto q: ampleNodesSuccs ) {
                    while ( !q.empty() ) {
                        q.front().free( alloc._pool );
                        q.pop_front();
                    }
                }
                return false;
            }
            index++;
        }
        for ( auto i: indepSet ) {
            yieldSuccessor( from, i, [&]( Node n ) { currentNode = n; } );
            auto ampleIt = ampleSet.cbegin();
            auto ampleNSIt = ampleNodesSuccs.begin();
            processConts(
                currentNode,
                [&]( dve::System::Continuation p ) {
                    if ( ampleIt == ampleSet.cend() || *ampleIt != p ) {
                        ok = false;
                        return false;
                    }
                    assert( ampleNSIt != ampleNodesSuccs.end() );
                    assert( !ampleNSIt->empty() );
                    yieldSuccessor(
                        currentNode,
                        p,
                        [&]( Node n ) {
                            if ( n.compare( ampleNSIt->front(), this->alloc._slack, n.size() ) != 0 )
                                ok = false;
                            ampleNSIt->front().free( alloc._pool );
                            ampleNSIt->pop_front();
                            n.free( alloc._pool );
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
            currentNode.free( alloc._pool );
            if ( !ok || ampleIt != ampleSet.cend() ) {
                for ( auto q: ampleNodesSuccs ) {
                    while ( !q.empty() ) {
                        q.front().free( alloc._pool );
                        q.pop_front();
                    }
                }
                return false;
            }
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

    template< typename Yield >
    void initials( Yield yield ) {
        Blob b = alloc.new_blob( stateSize() );
        updateMem( b );
        system->initial( ctx );
        yield( Node(), b, Label() );
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

    template< typename Y >
    void properties( Y yield ) {
        assert( system );
        yield( "deadlock", "deadlock freedom", PT_Deadlock );
        yield( "assert", "assertion safety", PT_Goal );
        if ( system->property ) /* FIXME. Bogus. */
            yield( "LTL", "Büchi neverclaim property", PT_Buchi );
        for ( auto p : system->properties )
            yield( wibble::str::fmt( p.id ), "Büchi neverclaim property", PT_Buchi );
    }

    void useProperty( std::string n ) {
        system->property = NULL;
        for ( auto &p : system->properties )
            if ( n == wibble::str::fmt( p.id ) )
                system->property = &p;
    }

    ReductionSet useReductions( ReductionSet r ) {
        if ( r.count( R_POR ) )
            return ReductionSet( { R_POR } );
        return ReductionSet();
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
