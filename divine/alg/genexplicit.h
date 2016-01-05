// -*- C++ -*- (c) 2013, 2015 Vladimír Štill <xstill@fi.muni.cz>

#if ALG_EXPLICIT

#include <unistd.h>
#include <errno.h>
#include <cstdint>
#include <algorithm>
#include <limits>

#include <brick-string.h>

#include <divine/algorithm/common.h>
#include <divine/algorithm/metrics.h>
#include <divine/explicit/explicit.h>
#include <divine/explicit/transpose.h>
#include <divine/graph/probability.h>

#ifndef DIVINE_ALGORITHM_COMPACT_H
#define DIVINE_ALGORITHM_COMPACT_H

namespace divine {
namespace algorithm {

struct GenExplicitShared : algorithm::Statistics {
    enum class Iteration : int8_t {
        Start = 0,
        Count,
        Normalize,
        PrececessorTracking,
        WriteFile
    };

    Iteration iteration;
    bool need_expand;

    GenExplicitShared() :
        iteration( Iteration::Count ), need_expand( false )
    { }

    template< typename BS >
    friend typename BS::bitstream &operator<<( BS &bs, GenExplicitShared s ) {
        bs << static_cast< int8_t >( s.iteration )
           << s.need_expand;
        return bs;
    }

    template< typename BS >
    friend typename BS::bitstream &operator>>( BS &bs, GenExplicitShared &s ) {
        int8_t iter;
        bs >> iter
           >> s.need_expand;
        s.iteration = Iteration( iter );
        return bs;
    }
};

template< typename Setup >
struct _GenExplicit : Algorithm, AlgorithmUtils< Setup, GenExplicitShared >,
                      Parallel< Setup::template Topology, _GenExplicit< Setup > >
{
    using This = _GenExplicit< Setup >;
    using Shared = GenExplicitShared;
    using Utils = AlgorithmUtils< Setup, Shared >;
    using Iteration = typename Shared::Iteration;

    struct ThreadLimit {
        int64_t indexStart; // inclusive
        int64_t indexEnd; // exclusive

        ThreadLimit( int64_t indexStart, int64_t indexEnd )
            : indexStart( indexStart ), indexEnd( indexEnd )
        { }
        ThreadLimit() = default;

        template< typename BS >
        friend typename BS::bitstream &operator<<( BS &bs, ThreadLimit tl ) {
            return bs << tl.indexStart << tl.indexEnd;
        }

        template< typename BS >
        friend typename BS::bitstream &operator>>( BS &bs, ThreadLimit &tl ) {
            return bs >> tl.indexStart >> tl.indexEnd;
        }
    };

    struct Params {
        int ringId;
        std::string path;
        bool forward;
        bool saveNodes;

        Params() : ringId( 0 ), path(), forward( false ), saveNodes( true ) { }

        template< typename BS >
        friend typename BS::bitstream &operator<<( BS &bs, Params p ) {
            return bs << p.ringId
                      << p.path
                      << p.forward
                      << p.saveNodes;
        }

        template< typename BS >
        friend typename BS::bitstream &operator>>( BS &bs, Params &p ) {
            return bs >> p.ringId
                      >> p.path
                      >> p.forward
                      >> p.saveNodes;
        }

        Params operator+( int i ) const {
            Params p = *this;
            p.ringId += i;
            return p;
        }
    };

    struct Limits : public std::vector< ThreadLimit > {
        int64_t nodesSize;

        Limits() : std::vector< ThreadLimit >( 0 ), nodesSize( 0 ) { }

        template< typename BS >
        friend typename BS::bitstream &operator<<( BS &bs, const Limits &s ) {
            bs << int( s.size() );
            for ( auto tl : s )
                bs << tl;
            bs << s.nodesSize;
            return bs;
        }

        template< typename BS >
        friend typename BS::bitstream &operator>>( BS &bs, Limits &s ) {
            int size;
            bs >> size;
            s.clear();
            s.reserve( size );
            for ( int i = 0; i < size; ++i ) {
                ThreadLimit tl;
                bs >> tl;
                s.push_back( tl );
            }
            bs >> s.nodesSize;
            return bs;
        }

        Limits &add( int ringId, int64_t nodesCnt, int64_t size ) {
            (*this)[ ringId ] = ringId == 0
                ? ThreadLimit( 1, 1 + nodesCnt )
                : ThreadLimit( (*this)[ ringId - 1 ].indexEnd,
                        (*this)[ ringId - 1 ].indexEnd + nodesCnt );
            nodesSize += size;
            return *this;
        }
    };

    ALGORITHM_CLASS( Setup );
    BRICK_RPC( Utils, &This::_init, &This::_explore,
                      &This::_por, &This::_por_worker, &This::_normalize,
                      &This::_trackPredecessors, &This::_writeFile, &This::_collectCount,
                      &This::_setLimits, &This::_getNodeId, &This::_cleanup );

    using Utils::shared;
    using Utils::shareds;

    Params params;
    Limits limits;
    int64_t nodes;
    int64_t nodesSize;

    // use tuple to save space if Label is empty
    struct EdgeSpec : std::tuple< int64_t, Label > {
        int64_t &index() { return std::get< 0 >( *this ); }
        Label &label() { return std::get< 1 >( *this ); }

        EdgeSpec( int64_t index, Label label )
            : std::tuple< int64_t, Label >( index, label )
        { }
    };

    struct Extension {

        int64_t index() { return get< 2 >( _cntrl ); }
        void index( int64_t ix ) { get< 2 >( _cntrl ) = ix; }

        void lock() { get< 0 >( _cntrl ).lock(); }
        void unlock() { get< 0 >( _cntrl ).unlock(); }

        int64_t &predCount() {
            ASSERT( iteration() < Iteration::PrececessorTracking );
            return _preds.predCount;
        }

        void mkPredVector( Pool &pool ) {
            if ( _preds.predCount > 0 ) {
                Blob preds = pool.allocate( _preds.predCount * sizeof( EdgeSpec ) );
                EdgeSpec *ptr = &pool.template get< EdgeSpec >( preds );
                for ( int64_t i = 0; i < _preds.predCount; ++i )
                    new ( &ptr[ i ] ) EdgeSpec( -1, Label() );
                _preds.predecessors = preds;
            } else
                _preds.predecessors = Blob();
        }

        int64_t predSize( Pool &pool ) {
            ASSERT( iteration() >= Iteration::Normalize );
            return pool.valid( _preds.predecessors ) ? pool.size( _preds.predecessors ) : 0;
        }

        void addPredecessor( Pool &pool, EdgeSpec predecessor ) {
            ASSERT( iteration() == Iteration::PrececessorTracking );
            ASSERT_NEQ( predecessor.index(), -1 );
            EdgeSpec *ptr = &pool.template get< EdgeSpec >( _preds.predecessors );
            for ( ; ptr->index() != -1; ++ptr ){
                if ( ptr >= reinterpret_cast< EdgeSpec * >( pool.dereference( _preds.predecessors ) + pool.size( _preds.predecessors ) ) )
                {
                    EdgeSpec *ptr2 = &pool.template get< EdgeSpec >( _preds.predecessors );
                    std::cout << index() << ": ";
                    for ( ; ptr2 < ptr; ++ptr2 )
                        std::cout << ptr2->index() << " ";
                    std::cout << "; " << predecessor.index() << std::endl;
                }
                ASSERT_LEQ( ptr, reinterpret_cast< EdgeSpec * >(
                        pool.dereference( _preds.predecessors )
                        + pool.size( _preds.predecessors ) ) - 1 );
            }
            *ptr = predecessor;
        }

        const EdgeSpec *predecessors( Pool &pool ) {
            ASSERT( iteration() >= Iteration::Normalize );
            return &pool.template get< EdgeSpec >( _preds.predecessors );
        }

        void dropPreds( Pool &pool ) {
            pool.free( _preds.predecessors );
        }

        auto iteration() -> typename brick::bitlevel::BitField< Iteration, 3 >::Virtual {
            return get< 1 >( _cntrl );
        }

        Extension() { }

      private:
        BitTuple<
            BitLock,
            BitField< Iteration, 3 >,
            BitField< int64_t, 60 >
        > _cntrl;

        union D {
            Blob predecessors;
            int64_t predCount;

            D() : predCount( 0 ) { }
        } _preds;
    };

    using Guard = typename Store::template Guard< Extension >;

    Extension &extension( Vertex v ) {
        return v.template extension< Extension >();
    }

    int64_t index( Vertex v ) {
        return this->store().valid( v )
            ? extension( v ).index()
            : 0;
    }

    Pool& pool() {
        return this->graph().pool();
    }

    visitor::TransitionAction updateIteration( Vertex t ) {
        Iteration old = extension( t ).iteration();
        extension( t ).iteration() = shared.iteration;
        return old != shared.iteration
            ? visitor::TransitionAction::Expand
            : visitor::TransitionAction::Forget;
    }

    struct Explore : Visit< This, Setup > {
        static visitor::ExpansionAction expansion( This &c, const Vertex &st )
        {
            c.shared.addNode( c.graph(), st );

            Guard _( st );
            c.extension( st ).iteration() = Iteration::Count;
            return visitor::ExpansionAction::Expand;
        }

        static visitor::TransitionAction transition( This &c, Vertex from, Vertex to, Label )
        {
            if ( from.valid() ) {
                Guard _( to );
                ++c.extension( to ).predCount();
            }
            c.shared.addEdge( c.store(), from, to );
            c.graph().porTransition( c.store(), from, to );
            return visitor::TransitionAction::Follow;
        }

        static visitor::DeadlockAction deadlocked( This &c, Vertex ) {
            c.shared.addDeadlock();
            return visitor::DeadlockAction::Ignore;
        }
    };

    void _explore() { // parallel
        this->visit( this, Explore(), shared.need_expand );
    }

    void _por_worker() {
        this->graph()._porEliminate( *this );
    }

    Shared _por( Shared sh ) {
        shared = sh;
        if ( this->graph().porEliminate( *this, *this ) )
            shared.need_expand = true;
        return shared;
    }

    void _count() {
        ASSERT( shared.iteration == Iteration::Count );
        for ( auto st : this->store() ) {
            // note: Iteration over store has static partitioning even in
            // shared store, this partitioning is preserved unless store
            // is resized. Therefore no Guard is needed.
            ASSERT( extension( st ).iteration() == Iteration::Count );
            ++nodes;
            nodesSize += this->pool().size( st.node() ) - sizeof( Extension );
        }
    }

    void _normalize() {
        ASSERT( shared.iteration == Iteration::Normalize );
        int64_t index = limits[ params.ringId ].indexStart;
        for ( auto st : this->store() ) {
            // note: Iteration over store has static partitioning even in
            // shared store, this partitioning is preserved unless store
            // is resized. Therefore no Guard is needed.
            ASSERT( extension( st ).iteration() == Iteration::Count );
            extension( st ).iteration() = Iteration::Normalize;
            extension( st ).index( index++ );
            ASSERT_LEQ( index, limits[ params.ringId ].indexEnd );

            extension( st ).mkPredVector( this->pool() );
        }
        ASSERT_EQ( index, limits[ params.ringId ].indexEnd );
    }

    struct TrackPredecessors : Visit< This, Setup > {
        static visitor::ExpansionAction expansion( This &, const Vertex & )
        {
            return visitor::ExpansionAction::Expand;
        }

        static visitor::TransitionAction transition( This &c, Vertex from, Vertex to, Label label )
        {
            Guard _( to );
            auto act = c.updateIteration( to );
            if ( from.valid() )
                c.extension( to ).addPredecessor( c.pool(), { c.index( from ), label } );
            return act;
        }

        static visitor::DeadlockAction deadlocked( This &, Vertex ) {
            return visitor::DeadlockAction::Ignore;
        }
    };

    void _trackPredecessors() {
        this->visit( this, TrackPredecessors() );
    }

    _GenExplicit( Meta m, typename Setup::Generator *g = nullptr )
        : Algorithm( m, sizeof( Extension ) ),
          params(), limits(), nodes( 0 ), nodesSize( 0 )
    {
        this->init( *this, g );

        if ( m.output.file.empty() ) {
            std::string basename = brick::string::basename( m.input.model );
            std::string output = basename + dess::extension;
            for ( int i = 0; access( output.c_str(), F_OK ) == 0; ++i ) {
                std::stringstream ss;
                ss << basename << "-" << i << dess::extension;
                output = ss.str();
            }
            params.path = output;
        } else
            params.path = m.output.file;

        params.saveNodes = m.output.saveStates;
        params.forward = true;
    }

    _GenExplicit( _GenExplicit &master, std::pair< int, int > id ) :
        Algorithm( master.meta(), sizeof( Extension ) ),
        params(), limits(), nodes( 0 ), nodesSize( 0 )
    {
        this->init( *this, master, id );
    }

    void collect() {
        for ( int i = 0; i < int( shareds.size() ); ++i )
            shareds[ i ].update( meta().statistics );
    }

    Limits _collectCount( Limits lim ) {
        return lim.add( params.ringId, nodes, nodesSize );
    }

    void _setLimits( Limits lim ) {
        limits = lim;
    }

    Params _init( Params init ) {
        params = init;
        return params + 1;
    }

    void run() {
        ring( params, &This::_init );
        params.ringId = -1;

        progress() << "  exploring vertices... \t " << std::flush;
        explore();
        result().fullyExplored = meta::Result::R::Yes;
        progress() << "done" << std::endl;

        progress() << "  counting vertices... \t\t " << std::flush;
        count();
        progress() << "found " << nodes << " states, "
                   << nodesSize << " bytes" << std::endl;
        ASSERT_EQ( nodes, meta().statistics.visited );

        progress() << "  normalization...   \t\t " << std::flush;
        normalize();
        progress() << "done" << std::endl;

        progress() << "  tracking predecessors... \t " << std::flush;
        trackPredecessors();
        progress() << "done" << std::endl;

        shared.update( meta().statistics );

        progress() << "  writing file... \t\t " << std::flush;
        writeFile();
        progress() << "done (" << params.path << ")" << std::endl;

        progress() << "  cleaning memory... \t\t " << std::flush;
        parallel( &This::_cleanup );
        progress() << "done" << std::endl;
    }

    void explore() {
        shared.iteration = Iteration::Count;
        parallel( &This::_explore );
        collect();

        do {
            shared.need_expand = false;
            ring( &This::_por );
            if ( shared.need_expand ) {
                parallel( &This::_explore );
                collect();
            }
        } while ( shared.need_expand );
    }

    void count() {
        ASSERT( shared.iteration == Iteration::Count );
        parallel( &This::_count );
        limits.resize( shareds.size() );
        limits = ring( limits, &This::_collectCount );
        this->topology().distribute( limits, &This::_setLimits );
        nodesSize = limits.nodesSize;
        nodes = limits.back().indexEnd - 1;
    }

    void normalize() {
        ASSERT( shared.iteration == Iteration::Count );
        shared.iteration = Iteration::Normalize;
        parallel( &This::_normalize );
    }

    void trackPredecessors() {
        ASSERT( shared.iteration == Iteration::Normalize );
        shared.iteration = Iteration::PrececessorTracking;
        parallel( &This::_trackPredecessors );
    }

    void writeFile() {
        ASSERT( shared.iteration == Iteration::PrececessorTracking );
        shared.iteration = Iteration::WriteFile;

        std::vector< EdgeSpec > initials;
        this->graph().initials( LongTerm(), [&]( Node, Node n, Label l ) {
            int64_t id;
            std::tie( std::ignore, id ) = ring( std::tuple< Node, int64_t >( n, -1 ),
                                                &This::_getNodeId );
            LongTerm().drop( this->pool(), n );
            ASSERT_NEQ( -1 /* not found */, id );
            ASSERT_LEQ( 1, id );
            initials.emplace_back( id, l );
        } );

        int flagNamesLength = 0;
        std::vector< std::string > flagNames;
        flagNames.emplace_back( "o:accepting" );
        flagNames.resize( graph::flags::firstAvailable );
        this->graph().enumerateFlags( [&]( std::string name, int, graph::flags::Type t ) {
                auto fname = graph::flags::flagName( name, t );
                if ( flagNames.size() >= 64 ) {
                    progress() << "WARNING: ignoring flag " << fname
                             << ", you can save at most 64 flags" << std::endl;
                    return;
                }
                flagNames.emplace_back( std::move( fname ) );
            } );
        for ( auto fname : flagNames )
            flagNamesLength += fname.size();

        ASSERT_EQ( nodes, meta().statistics.visited );
        auto creator = dess::preallocate( params.path )
            .nodes( nodes + 1 ) // pseudo-initial
            .edges( meta().statistics.transitions + initials.size() )
            .forward()
            .backward()
            .generator( meta().input.modelType )
            .labelSize( Label() )
            .saveFlags( flagNames.size(), flagNamesLength );
        if ( params.saveNodes )
            creator.saveNodes( nodesSize );
        if ( std::is_same< Label, uint64_t >::value )
            creator.uint64Labels();
        if ( std::is_same< Label, graph::Probability >::value )
            creator.probability();

        auto dess = creator();

        auto sfIns = dess.stateFlags.flagMap.inserter();
        for ( auto f : flagNames )
            sfIns.emplace( f.size(), [&]( char *ptr, int64_t ) {
                        std::copy( f.begin(), f.end(), ptr );
                    } );

        if ( params.saveNodes )
            dess.nodes.inserter().emplace( 0, []( char *, int64_t ) { } );
        dess.backward.inserter().emplace(
            sizeof( EdgeSpec ) * initials.size(),
                [ &initials ]( char *ptr, int64_t ) {
                    std::copy( initials.begin(), initials.end(),
                        reinterpret_cast< EdgeSpec * >( ptr ) );
                } );

        auto r = ring( true, &This::_writeFile );
        ASSERT( r );
        static_cast< void >( r );
    }

    std::tuple< Node, int64_t > _getNodeId( std::tuple< Node, int64_t > q ) {
        Node n;
        int64_t ix;
        std::tie( n, ix ) = q;
        ASSERT( pool().valid( n ) );
        if ( ix < 0 && this->store().knows( n ) ) {
            Vertex v = this->store().fetch( n );
            ix = extension( v ).index();
        }
        return std::make_tuple( n, ix );
    }

    bool _writeFile( bool ok ) {
        ASSERT( ok );

        return  params.saveNodes
            ? _writeFileT< true >()
            : _writeFileT< false >();
    }

    template< bool saveStates >
    bool _writeFileT() {
        dess::Explicit dess( params.path, dess::Explicit::OpenMode::Write );
        auto start = limits[ params.ringId ].indexStart;
        ASSERT_LEQ( 1, start );
        auto edgeInserter = dess.backward.inserter( start );
        auto nodeInserter = dess.nodes.inserter( start );
        uint64_t *flags = dess.stateFlags.flagMasks + start;

        graph::FlagVector allflags;
        allflags.emplace_back( graph::flags::accepting );
        allflags.resize( graph::flags::firstAvailable );
        this->graph().enumerateFlags( [&]( std::string, int i, graph::flags::Type ) {
                    allflags.emplace_back( i );
                } );

        for ( auto st : this->store() ) {
            ASSERT( extension( st ).iteration() == Iteration::PrececessorTracking );
            auto &ext = extension( st );
            edgeInserter.emplace( ext.predSize( pool() ),
                [&]( char *ptr, int64_t size ) {
                    if ( size )
                        std::copy( ext.predecessors( this->pool() ),
                            ext.predecessors( this->pool() )
                                + size / sizeof( EdgeSpec ),
                            reinterpret_cast< EdgeSpec * >( ptr ) );
                } );
            if ( saveStates ) {
                char *source = this->pool().dereference( st.node() ) + sizeof( Extension );
                nodeInserter.emplace( pool().size( st.node() ) - sizeof( Extension ),
                    [ source ]( char *ptr, int64_t size ) {
                        std::copy( source, source + size, ptr );
                    } );
            }

            auto filtered = this->graph().stateFlags( st.node(), allflags );
            for ( auto f : filtered ) {
                for ( int i = 0; i < allflags.size(); ++i )
                    if ( f == allflags[ i ] ) {
                        *flags |= 1 << i;
                        break;
                    }
            }
            ++flags;
        }
        return true;
    }

    // after this store must NOT be accessed
    void _cleanup() {
        for ( auto st : this->store() ) {
            extension( st ).dropPreds( this->pool() );
        }
    }
};

template< typename Setup >
struct _GenExplicitTranspose : Algorithm, Sequential
{
    std::string path;

    _GenExplicitTranspose( Meta &meta, std::string path ) :
        Algorithm( meta ),
        path( path )
    { }

    void run() {
        progress() << "  transposing graph... \t\t " << std::flush;
        transpose();
        progress() << "done" << std::endl;
    }

    void transpose() {
        dess::Explicit dess( path, dess::Explicit::OpenMode::Write );
        dess::transpose< typename _GenExplicit< Setup >::EdgeSpec >(
                dess.backward, dess.forward );
    }
};

template< typename Setup >
struct GenExplicit : Algorithm {

    using Stage1 = _GenExplicit< Setup >;
    using Stage2 = _GenExplicitTranspose< Setup >;

    std::unique_ptr< Stage1 > stage1;
    std::unique_ptr< Stage2 > stage2;

    GenExplicit( Meta m, typename Setup::Generator *g = nullptr )
        : Algorithm( m ),
          stage1( new Stage1( m, g ) ),
          stage2()
    { }

    GenExplicit( GenExplicit &master, std::pair< int, int > id ) :
        stage1( new Stage1( master._stage1, id ) ),
        stage2()
    { }

    void run() {
        stage1->run();
        meta() = stage1->meta();
        stage2 = std::unique_ptr< Stage2 >(
                new Stage2( meta(), stage1->params.path ) );
        stage1 = nullptr;
        stage2->run();
        meta() = stage2->meta();
    }
};

}
}

#endif // DIVINE_ALGORITHM_COMPACT_H
#endif
