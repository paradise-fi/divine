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
Model is read from *.xml file and one or more LTL properties are read from *.ltl file with the same name.
LTL properties are referred to using numbers, starting from 0.
When the implicit property, the deadlock freedom, is verified, error states (out-of-range assignments,
division by zero and negative clock assignments) are marked as goals.
If the LU transformation is enabled, time-lock detection is turned off, since it
would produce false positives.
When an LTL property is verified, the transformation to exclude Zeno runs can be used.
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
    bool hasLTL = false;
    bool nonZeno = false;

    template< typename Yield >
    void successors( Node from, Yield yield ) {
        assert( from.valid() );
        unsigned int nSuccs = 0;
        const std::vector< std::pair< int, int > >& btrans = buchi.transitions( gen.getPropLoc( mem( from ) ) );

        auto callback = [ this, &btrans, &nSuccs, yield ] ( const char* succ, const TAGen::EdgeInfo* ) mutable {
            for ( auto btr = btrans.begin(); btr != btrans.end(); ++btr ) {
                Node n = newNode( succ );
                if ( doBuchiTrans( mem( n ), *btr ) ) {
                    yield( n, Label() );
                } else {
                    n.free( pool() );
                }
            }
            nSuccs++;
        };

        gen.genSuccs( mem( from ), callback );
        // in case of deadlock or time-lock, allow the property automaton to move on its own if veryfing LTL
        if ( hasLTL && nSuccs == 0 ) {
            callback( mem( from ), NULL );
        }
    }

    bool isAccepting( Node n ) {
        if ( gen.isErrState( mem( n ) ) )
            return false;
        return buchi.isAccepting( gen.getPropLoc( mem( n ) ) );
    }

    // error states are goals, timelocks just show up as deadlocks
    bool isGoal( Node n ) {
        return gen.isErrState( mem( n ) );
    }

    std::string showNode( Node n ) {
        return gen.showNode( mem( n ) );
    }

    std::string showTransition( Node from, Node to, Label ) {
        assert( from.valid() );

        std::string str;
        const std::vector< std::pair< int, int > >& btrans = buchi.transitions( gen.getPropLoc( mem( from ) ) );
        std::vector< char > tmp( gen.stateSize() );
        char* copy = &tmp[0];

        auto callback = [ this, &btrans, &str, to, copy ] ( const char* succ, const TAGen::EdgeInfo* e ) {
            if ( !str.empty() ) return;
            for ( auto btr = btrans.begin(); btr != btrans.end(); ++btr ) {
                memcpy( copy, succ, gen.stateSize() );
                if ( !doBuchiTrans( copy, *btr ) ) continue;
                if ( memcmp( copy, mem( to ), gen.stateSize() ) == 0 ) {
                    str = buildEdgeLabel( e, btr->first );
                    break;
                }
            }
        };

        gen.genSuccs( mem( from ), callback );
        // if we verify LTL, automaton can move on its own is system is deadlocked
        if ( hasLTL && str.empty() ) {
            callback( mem( from ), NULL );
        }
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

    void read( std::string file, Timed * = nullptr ) {
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
        int propId;

        if ( n == "deadlock" )
            propId = -1;
        else {
            propId = std::atoi( n.c_str() );
            if ( propId >= int( ltlProps.size() ) || propId < 0 )
                throw wibble::exception::Consistency( "Unknown property " + n +
                                                      ". Please consult divine info." );
        }

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

        if ( nonZeno && hasLTL ) {
            // excludeZeno was called first
            nonZenoBuchi();
        }
    }

    template< typename Y >
    void properties( Y yield ) {
        yield( "deadlock", "deadlock freedom", PT_Deadlock );
        for ( unsigned int i = 0; i < ltlProps.size(); i++ ) {
            yield( std::to_string( i ), ltlProps[ i ], PT_Buchi );
        }
    }

    ReductionSet useReductions( ReductionSet r ) override {
        ReductionSet applied;
        if ( r.count( R_LU ) ) {
            gen.enableLU( true );
            applied.insert( R_LU );
        }
        return applied;
    }

    // transforms the Buchi automaton so that no zeno runs can be accepting,
    void excludeZeno() {
        if ( !nonZeno && hasLTL ) {
            // useProperty was already called
            nonZenoBuchi();
        }
        nonZeno = true;
    }

	// for timed automata, enabling fairness actually performs Zeno reduction
	void fairnessEnabled( bool enabled ) {
		if ( enabled )
			excludeZeno();
		assert( nonZeno == enabled );
	}

private:
    char* mem( Node s ) const {
        return &s.get< char >( alloc._slack );
    }

    Node newNode( const char* src ) {
        Node n = alloc.new_blob( gen.stateSize() );
        memcpy( mem( n ), src, gen.stateSize() );
        return n;
    }

    // tries to perform Buchi transition, returs true on succes, false if this transiiton is blocked,
    // true is also returned for error states, but the automaton is not affected
    bool doBuchiTrans( char* node, const std::pair< int, int >& btr ) {
        if ( gen.isErrState( node ) )
            return true;
        if ( gen.evalPropGuard( node, propGuards[ btr.second ] ) ) {
            gen.setPropLoc( node, btr.first );
            // if doing non-zeno reduction, and an accepting state is reached, reset the auxiliary clock
            if ( nonZeno && buchi.isAccepting( btr.first ) ) {
                gen.resetAuxClock();
            }
            return true;
        }
        return false;
    }

    std::string buildEdgeLabel( const TAGen::EdgeInfo* e, int propGuard = -1 ) {
        std::stringstream ss;
        if ( e ) {
            if ( e->syncType >= 0 )
                ss << e->sync.toString() << ( e->syncType == UTAP::Constants::SYNC_QUE ? "?; " : "!; " );
            ss << e->guard.toString();
            if ( e->assign.toString() != "1" )
                ss << "; " << e->assign.toString();
        } else
            ss << "time";
        if ( propGuard > 0 ) {
            ss << " [" << propGuards[ propGuard ].toString() << "]";
        }
        assert( !ss.str().empty() );
        return ss.str();
    }

    // to eliminate Zeno runs, we transform the Buchi automaton
    // to ensure that at least one time unit passes in every accepting cycle
    void nonZenoBuchi() {
        gen.addAuxClock();
        const int oldSize = buchi.size();
        for ( int i = 0; i < oldSize; i++ ) {
            if ( buchi.isAccepting( i ) ) {
                // create accepting copy
                int copy = buchi.duplicateState( i );
                buchi.setAccepting( i, false );
                // remove each edge j --(g)--> i, add edges j --(g && aux<1)--> i and j --(g && aux>=1)--> copy
                for ( int j = 0; j < buchi.size(); j++ ) {
                    auto& tr = buchi.editableTrans( j );
                    const unsigned int nTrans = tr.size();
                    for ( unsigned int t = 0; t < nTrans; t++ ) {
                        if ( tr[ t ].first == i ) {
                            auto auxGuard = gen.addAuxToGuard( propGuards[ tr[ t ].second ] );
                            // guard of the original edge is changed to "g && aux<1"
                            propGuards.push_back( auxGuard.first );
                            tr[ t ].second = propGuards.size() - 1;
                            // new edge leading to the accepting copy has guard "g && aux>=1"
                            propGuards.push_back( auxGuard.second );
                            tr.emplace_back( copy, propGuards.size() - 1 );
                        }
                    }
                }
            }
        }
    }
};

}
}

#endif
#endif
