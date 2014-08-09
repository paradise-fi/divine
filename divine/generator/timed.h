// -*- C++ -*-

#ifdef O_TIMED
#ifndef DIVINE_GENERATOR_TIMED_H
#define DIVINE_GENERATOR_TIMED_H

#include <stdexcept>
#include <cstdlib>
#include <divine/generator/common.h>
#include <divine/utility/buchi.h>
#include <divine/timed/gen.h>
#include <divine/toolkit/bitoperations.h>

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
        assert( this->pool().valid( from ) );
        unsigned int nSuccs = 0;
        const std::vector< std::pair< int, int > >& btrans = buchi.transitions( gen.getPropLoc( mem( from ) ) );

        auto callback = [ this, &btrans, &nSuccs, yield ] ( const char* succ, const TAGen::EdgeInfo* ) mutable {
            for ( auto btr = btrans.begin(); btr != btrans.end(); ++btr ) {
                Node n = newNode( succ );
                if ( doBuchiTrans( mem( n ), *btr ) ) {
                    yield( n, Label() );
                } else {
                    pool().free( n );
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
        assert( this->pool().valid( from ) );

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
        Node n = makeBlobCleared( gen.stateSize() );
        gen.initial( mem( n ) );
        if ( !gen.isErrState( mem( n ) ) )
            gen.setPropLoc( mem( n ), buchi.getInitial() );
        yield( Node(), n, Label() );
    }

    void release( Node s ) { pool().free( s ); }

    void read( std::string file, std::vector< std::string > /* definitions */,
            Timed * = nullptr )
    {
        gen.read( file );

        // replace extension with .ltl and try to read properties
        size_t pos = file.find_last_of( "./\\" );
        std::string fileBase =
            ( pos != std::string::npos && file[ pos ] == '.')
            ? file.substr( 0, pos ) : file;
        Buchi::readLTLFile( fileBase + ".ltl", ltlProps, ltlDefs );
    }

    void useProperties( PropertySet s ) {

        if ( s.size() != 1 )
            throw wibble::exception::Consistency( "Timed only supports singleton properties" );

        propGuards.resize( 1 );
        int propId;

        std::string n = *s.begin();

        if ( n == "deadlock" )
            propId = -1;
        else {
            propId = std::atoi( n.c_str() );
            if ( propId >= int( ltlProps.size() ) || propId < 0 )
                throw wibble::exception::Consistency( "Unknown property " + n +
                                                      ". Please consult divine info." );
        }

        hasLTL = false;
        if ( propId >= 0 && propId < intptr_t( ltlProps.size() ) )  {
            hasLTL = buchi.build( ltlProps[ propId ],
                [this]( const Buchi::DNFClause& clause ) -> int {
                    if ( clause.empty() )
                        return 0;
                    this->propGuards.push_back( this->gen.buildPropGuard( clause, this->ltlDefs ) );
                    return this->propGuards.size() - 1;
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

        for ( const auto &pg : propGuards )
            gen.parsePropClockGuard( pg );
        gen.finalizeUra();
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
            applied.insert( R_LU );
        } else {
            gen.enableLU( false );
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
        gen.finalizeUra();
	}

    template< typename Coroutine >
    void splitHint( Coroutine &cor, int a, int b, int ch ) {
        Common::splitHint( cor, a, b, ch );
    }

    template< typename Coroutine >
    void splitHint( Coroutine &cor ) {
        if ( cor.unconsumed() <= Common::SPLIT_LIMIT ) {
            cor.consume( cor.unconsumed() );
            return;
        }

        auto splits = gen.getSplitPoints();
        cor.split( 2 );

        cor.split( 2 );
        cor.consume( splits[ 0 ] );
        cor.consume( splits[ 1 ] - splits[ 0 ] );
        cor.join();

        // run the binary splitter from Common on the rest
        Common::splitHint( cor, cor.unconsumed(), 0, splits[ 2 ] );
        cor.join();
    }

private:
    char* mem( Node s ) {
        return &pool().get< char >( s, this->slack() );
    }

    Node newNode( const char* src ) {
        Node n = makeBlob( gen.stateSize() );
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
        const size_t oldSize = buchi.size();
        for ( size_t i = 0; i < oldSize; i++ ) {
            if ( buchi.isAccepting( i ) ) {
                // create accepting copy
                int copy = buchi.duplicateState( i );
                buchi.setAccepting( i, false );
                // remove each edge j --(g)--> i, add edges j --(g && aux<1)--> i and j --(g && aux>=1)--> copy
                for ( size_t j = 0; j < buchi.size(); j++ ) {
                    auto& tr = buchi.editableTrans( j );
                    const size_t nTrans = tr.size();
                    for ( size_t t = 0; t < nTrans; t++ ) {
                        if ( tr[ t ].first == intptr_t( i ) ) {
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
