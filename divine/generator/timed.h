// -*- C++ -*-

#ifdef O_TIMED
#ifndef DIVINE_GENERATOR_TIMED_H
#define DIVINE_GENERATOR_TIMED_H
#include <stdexcept>
#include <cstdlib>
#include <divine/generator/common.h>
#include <divine/utility/buchi.h>
#include <divine/timed/gen.h>

/*
Interpreter of UPPAAL timed automata models.
Model is read from *.xml file and one or more LTL properties are read from *.ltl file with the same name. Properties are referred to using numbers, first property
(with number 0) is used by default. If the given ltl file is not found or property with negative or out-of-range number is selected, property "true" is used.
Error states, created by out-of-range assignments, division by zero and negative clock assignments are marked as goal for the reachability algorithm.
*/

namespace divine {
namespace generator {

struct Timed : public Common< Blob > {
    typedef Blob Node;
    typedef generator::Common< Blob > Common;
    TAGen gen;
    divine::Buchi buchi;
    std::vector< std::string > ltlProps;
    std::map< std::string, std::string > ltlDefs;
    std::vector< TAGen::PropGuard > propGuards;
    bool hasLTL;

    char* mem( Node s ) const {
        return &s.get< char >( alloc._slack );
    }

    template< typename Yield >
    void successors( Node from, Yield yield ) {
        assert( from.valid() );
        if ( gen.isErrState( mem( from ) ) )
            return;
        const std::vector< std::pair< int, int > >& btrans = buchi.transitions( gen.getPropLoc( mem( from ) ) );
        gen.genSuccs( mem( from ), [ this, &btrans, yield ] ( const char* succ, const TAGen::EdgeInfo* ) mutable {
            for ( auto btr = btrans.begin(); btr != btrans.end(); ++btr ) {
                Node n = alloc.new_blob( gen.stateSize() );
                memcpy( mem( n ), succ, gen.stateSize() );
                if ( gen.evalPropGuard( mem( n ), propGuards[ btr->second ] ) ) {
                    assert( memcmp( mem( n ), succ, gen.stateSize() ) == 0 );   // after guard evaluation, we should get empty zone or unchaged state
                    gen.setPropLoc( mem( n ), btr->first );
                    yield( n, Label() );
                } else {
                    n.free( pool() );
                }
            }
        });
    }

    bool isAccepting( Node n ) {
        return buchi.isAccepting( gen.getPropLoc( mem( n ) ) );
    }

    bool isGoal( Node n ) {
        int err = gen.isErrState( mem( n ) );
        return err && err != EvalError::TIMELOCK;
    }

    std::string showNode( Node n ) {
        return gen.showNode( mem( n ) );
    }

    std::string showTransition( Node from, Node to, Label ) {
        assert( from.valid() );

        std::string str;
        const std::vector< std::pair< int, int > >& btrans = buchi.transitions( gen.getPropLoc( mem( from ) ) );
        gen.genSuccs( mem( from ), [ this, &btrans, &str, to ] ( const char* succ, const TAGen::EdgeInfo* e ) {
            if ( !str.empty() ) return;
            std::vector< char > tmp( gen.stateSize() );
            char* copy = &tmp[0];
            for ( auto btr = btrans.begin(); btr != btrans.end(); ++btr ) {
                memcpy( copy, succ, gen.stateSize() );
                if ( !gen.evalPropGuard( copy, propGuards[ btr->second ] ) ) continue;
                gen.setPropLoc( copy, btr->first );
                if ( memcmp( copy, mem( to ), gen.stateSize() ) == 0 ) {
                    if ( e ) {
                        if ( e->syncType >= 0 )
                            str = e->sync.toString() + ( e->syncType == UTAP::Constants::SYNC_QUE ? "?; " : "!; " );
                        str += e->guard.toString();
                        if ( e->assign.toString() != "1" )
                            str += "; " + e->assign.toString();
                    } else
                        str = "time";
                    break;
                }
            }
        });
        assert( !str.empty() );
        return str;
    }

    template< typename Yield >
    void initials( Yield yield ) {
        Node n = alloc.new_blob( gen.stateSize() );
        gen.initial( mem( n ) );
        if ( !gen.isErrState( mem( n ) ) )
            gen.setPropLoc( mem( n ), buchi.getInitial() );
        yield( Node(), n, Label() );
    }

    void release( Node s ) { s.free( pool() ); }

    void read( std::string file ) {
        gen.read( file );

        // replace extension with .ltl and try to read properties
        size_t pos = file.find_last_of( "./\\" );
        std::string fileBase =
            ( pos != std::string::npos && file[ pos ] == '.')
            ? file.substr( 0, pos ) : file;
        Buchi::readLTLFile( fileBase + ".ltl", ltlProps, ltlDefs );
    }

    void useProperty( std::string n ) {
        propGuards.resize( 1 );

        int propId = (n == "deadlock") ? -1 : std::atoi( n.c_str() );
        if ( propId >= ltlProps.size() )
            throw wibble::exception::Consistency( "Unknown property " + n + ". Please consult divine info." );

        hasLTL = false;
        if ( propId >= 0 && propId < ltlProps.size() )  {
            hasLTL = buchi.build( ltlProps[ propId ],
                [this]( const Buchi::DNFClause& clause ) -> int {
                    if ( clause.empty() )
                        return 0;
                    propGuards.push_back( gen.buildPropGuard( clause, ltlDefs ) );
                    return propGuards.size() - 1;
                }
            );
        }
        if ( !hasLTL )
            buchi.buildEmpty();
        assert( buchi.size() > 0 );
    }

    template< typename Y >
    void properties( Y yield ) {
        std::stringstream ss;
        yield( "deadlock", "(deadlock reachability)", PT_Deadlock );
        for ( unsigned int i = 0; i < ltlProps.size(); i++ ) {
            ss.str( "" );
            ss << i;
            yield( ss.str(), ltlProps[ i ], PT_Buchi );
        }
    }
};

}
}

#endif
#endif
