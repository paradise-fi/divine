// -*- C++ -*- (c) 2007-2014 Petr Rockai <me@mornfall.net>
//             (c) 2012, 2013 Jan Kriho

#if GEN_DVE

#include <deque>
#include <vector>

#include <divine/dve/preprocessor.h>
#include <divine/dve/interpreter.h>
#include <divine/generator/common.h>
#include <wibble/exception.h>

#ifndef DIVINE_GENERATOR_DVE_H
#define DIVINE_GENERATOR_DVE_H

namespace divine {
namespace generator {

struct Dve : public Common< Blob > {
    std::shared_ptr< dve::System > system;

    struct dve::EvalContext ctx;
    typedef Blob Node;
    typedef generator::Common< Blob > Common;
    typedef typename Common::Label Label;

    template< typename Yield >
    void enabledConts( Node from, Yield yield ) {
        if ( !pool().valid( from ) )
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
        Blob b = makeBlob( stateSize() );
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
                this->yieldSuccessor( from, p, [&]( Node n ) { yield( n, Label() ); } );
                return true;
            }
        );
    }

    bool POR_C1( dve::Transition & ampleTrans, dve::Transition & indepTrans ) {
        if ( ampleTrans.pre.count( &indepTrans ) || ampleTrans.dep.count( &indepTrans ) )
            return false;
        return true;
    }

    bool POR_C2( dve::Transition & ampleTrans ) {
        if ( !system->property )
            return true;
        return !ampleTrans.visible[ system->property ];
    }

    template< typename Yield >
    void ample( Node from, Yield yield ) {
        std::deque< dve::System::Continuation > ampleCands;
        for ( int i = 0; i < system->processCount(); i++ ) {
            bool tryNext = false;
            processConts(
                from,
                [&]( dve::System::Continuation p ) {
                    if ( p.process >= this->system->processes.size() )
                        return true;
                    dve::Transition &ampleCandidate = this->system->getTransition( this->ctx, p );
                    if ( !POR_C2( ampleCandidate ) ) {
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
                    dve::Transition &indep = this->system->getTransition( this->ctx, p );
                    for ( dve::System::Continuation &ac : ampleCands ) {
                        dve::Transition &ampleCandidate = this->system->getTransition( this->ctx, ac );
                        if ( !POR_C1( ampleCandidate, indep ) ) {
                            tryNext = true;
                            return false;
                        }
                    }
                    return true;
                },
                i,
                false
            );

            if ( tryNext ) {
                ampleCands.clear();
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
        Blob b = makeBlobCleared( stateSize() );
        updateMem( b );
        system->initial( ctx );
        yield( Node(), b, Label() );
    }

    void read( std::string path, std::vector< std::string > definitions, Dve *blueprint = nullptr ) {
        if ( blueprint ) {
            system = blueprint->system;
            return;
        }
        std::unordered_map< std::string, dve::preprocessor::Definition > defs;
        try {
            for ( std::string & d : definitions ) {
                 dve::preprocessor::Definition def( d );
                 defs[ def.var ] = def;
            }
        }
        catch ( std::string error ) {
            std::cerr << error << std::endl;
            throw;
        }

        std::ifstream file;
        file.open( path.c_str() );
        dve::IOStream stream( file );
        dve::Lexer< dve::IOStream > lexer( stream );
        dve::Parser::Context ctx( lexer, path );
        try {
            dve::parse::System ast( ctx );
            dve::preprocessor::System preproc( defs );
            preproc.process( ast );
            ast.fold();
            system = std::make_shared< dve::System >( ast );
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
        int i = 0;
        for ( auto p : system->properties ) {
            yield( "LTL" + (i ? wibble::str::fmt( i ) : ""), "Büchi neverclaim property", PT_Buchi );
            ++ i;
        }
    }

    void useProperties( PropertySet s ) {
        if ( s.size() != 1 )
            throw wibble::exception::Consistency( "DVE only supports singleton properties" );

        std::string n = *s.begin();
        system->property = NULL;
        int i = 0;
        for ( auto &p : system->properties ) {
            if ( n == "LTL" + (i ? wibble::str::fmt( i ) : "") )
                system->property = &p;
            ++ i;
        }
    }

    ReductionSet useReductions( ReductionSet r ) {
        if ( r.count( R_POR ) ) {
            system->PORInit();
            return ReductionSet( { R_POR } );
        }
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
        return &pool().get< char >( s, slack() );
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
        if ( !pool().valid( from ) || !pool().valid( to ) )
            return "";

        std::string transLabel = "";

        enabledConts( from, [&]( dve::System::Continuation p ) {
                Blob b = this->makeBlob( stateSize() );
                memcpy( mem( b ), mem( from ), stateSize() );
                updateMem( b );

                this->system->apply( this->ctx, p );

                updateMem( from );
                if ( pool().equal( b, to, this->slack() ) ) {
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

    void release( Node s ) { pool().free( s ); }
    Dve() : system( 0 ) {}
};

}
}

#endif
#endif
