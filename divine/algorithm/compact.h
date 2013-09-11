// -*- C++ -*- (c) 2013 Vladimír Štill <xstill@fi.muni.cz>

#include <divine/algorithm/common.h>
#include <divine/algorithm/metrics.h>
#include <divine/compact/compact.h>
#include <divine/compact/transpose.h>
#include <divine/toolkit/probability.h>
#include <cstdint>
#include <algorithm>

#include <unistd.h>
#include <errno.h>

#ifndef DIVINE_ALGORITHM_COMPACT_H
#define DIVINE_ALGORITHM_COMPACT_H

namespace divine {
namespace algorithm {

template< typename Setup >
struct _Compact : Algorithm, AlgorithmUtils< Setup >,
                 Parallel< Setup::template Topology, _Compact< Setup > >
{
    enum class Iteration : int8_t {
        Start = 0,
        Count,
        Normalize,
        PrececessorTracking,
        WriteFile
    };

    typedef _Compact< Setup > This;

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

    struct Shared : algorithm::Statistics {
        Iteration iteration;
        bool need_expand;

        Shared() :
            iteration( Iteration::Count ), need_expand( false )
        { }

        template< typename BS >
        friend typename BS::bitstream &operator<<( BS &bs, Shared s ) {
            bs << static_cast< int8_t >( s.iteration )
               << s.need_expand;
            return bs;
        }

        template< typename BS >
        friend typename BS::bitstream &operator>>( BS &bs, Shared &s ) {
            int8_t iter;
            bs >> iter
               >> s.need_expand;
            s.iteration = static_cast< Iteration >( iter );
            return bs;
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

    ALGORITHM_CLASS( Setup, Shared );

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
        int64_t index;
        union D {
            Blob predecessors;
            int64_t predCount;

            D() : predCount( 0 ) { }
        } _data;
        int64_t &predCount() {
            assert( iteration < Iteration::PrececessorTracking );
            return _data.predCount;
        }
        int64_t predSize( Pool &pool ) const {
            assert( iteration >= Iteration::Normalize );
            return pool.size( _data.predecessors );
        }
        void addPredecessor( Pool &pool, EdgeSpec predecessor ) {
            assert( iteration == Iteration::PrececessorTracking );
            EdgeSpec *ptr = reinterpret_cast< EdgeSpec* >(
                    pool.dereference( _data.predecessors ) );
            for ( ; ptr->index() != -1; ++ptr )
                assert_leq( ptr, reinterpret_cast< EdgeSpec * >(
                        pool.dereference( _data.predecessors )
                        + pool.size( _data.predecessors ) ) - 1 );
            *ptr = predecessor;
        }
        const EdgeSpec *predecessors( Pool &pool ) const {
            assert( iteration >= Iteration::Normalize );
            return reinterpret_cast< EdgeSpec * >(
                    pool.dereference( _data.predecessors ) );
        }
        Iteration iteration;

        Extension() : index( 0 ), _data(), iteration( Iteration::Start ) { }
    };

    Extension &extension( Vertex v ) {
        return v.template extension< Extension >();
    }

    int64_t index( Vertex v ) {
        return this->store().valid( v )
            ? extension( v ).index
            : 0;
    }

    Pool& pool() {
        return this->graph().pool();
    }

    visitor::TransitionAction updateIteration( Vertex t ) {
        Iteration old = extension( t ).iteration;
        extension( t ).iteration = shared.iteration;
        return ( old != shared.iteration )
            ? visitor::TransitionAction::Expand
            : visitor::TransitionAction::Forget;
    }

    struct Count : Visit< This, Setup > {
        static visitor::ExpansionAction expansion( This &c, const Vertex &st )
        {
            c.shared.addNode( c.graph(), st );
            c.extension( st ).iteration = Iteration::Count;
            ++c.nodes;
            c.nodesSize += c.pool().size( st.node() ) - sizeof( Extension );
            return visitor::ExpansionAction::Expand;
        }

        static visitor::TransitionAction transition( This &c, Vertex from, Vertex to, Label )
        {
            ++c.extension( to ).predCount();
            c.shared.addEdge( c.store(), from, to );
            c.graph().porTransition( c.store(), from, to );
            return visitor::TransitionAction::Follow;
        }

        static visitor::DeadlockAction deadlocked( This &c, Vertex ) {
            c.shared.addDeadlock();
            return visitor::DeadlockAction::Ignore;
        }
    };

    void _count() { // parallel
        this->visit( this, Count(), shared.need_expand );
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

    void _normalize() {
        assert( shared.iteration == Iteration::Normalize );
        int64_t index = limits[ params.ringId ].indexStart;
        for ( auto st : this->store() ) {
            assert( extension( st ).iteration == Iteration::Count );
            extension( st ).iteration = Iteration::Normalize;
            extension( st ).index = index++;
            assert_leq( index, limits[ params.ringId ].indexEnd );

            int64_t predCount = extension( st ).predCount();
            Blob preds = pool().allocate( predCount * sizeof( EdgeSpec ) );
            EdgeSpec *ptr = reinterpret_cast< EdgeSpec* >( pool().dereference( preds ) );
            for ( int64_t i = 0; i < predCount; ++i )
                ptr[ i ] = EdgeSpec( -1, Label() );
            extension( st )._data.predecessors = preds;
        }
        assert_eq( index, limits[ params.ringId ].indexEnd );
    }

    struct TrackPredecessors : Visit< This, Setup > {
        static visitor::ExpansionAction expansion( This &c, const Vertex &st )
        {
            return visitor::ExpansionAction::Expand;
        }

        static visitor::TransitionAction transition( This &c, Vertex from, Vertex to, Label label )
        {
            auto act = c.updateIteration( to );
            c.extension( to ).addPredecessor( c.pool(),
                    EdgeSpec( c.index( from ), label ) );
            return act;
        }

        static visitor::DeadlockAction deadlocked( This &c, Vertex ) {
            return visitor::DeadlockAction::Ignore;
        }
    };

    void _trackPredecessors() {
        this->visit( this, TrackPredecessors() );
    }

    _Compact( Meta m ) : Algorithm( m, sizeof( Extension ) ),
        params(), limits(), nodes( 0 ), nodesSize( 0 )
    {
        this->init( *this );

        if ( m.output.file.empty() ) {
            std::string basename = wibble::str::basename( m.input.model );
            std::string output = basename + ".dcess";
            for ( int i = 0; access( output.c_str(), F_OK ) == 0; ++i ) {
                std::stringstream ss;
                ss << basename << "-" << i << ".dcess";
                output = ss.str();
            }
            params.path = output;
        } else
            params.path = m.output.file;

        params.saveNodes = m.output.saveStates;
        params.forward = true;
    }

    _Compact( _Compact &master, int id ) :
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
        this->topology().ring( params, &This::_init );
        params.ringId = -1;

        progress() << "  counting vertices... \t\t " << std::flush;
        count();
        result().fullyExplored = meta::Result::Yes;
        progress() << "found " << nodes << " states, "
                   << nodesSize << " bytes" << std::endl;
        assert_eq( nodes, meta().statistics.visited );

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

    void count() {
        shared.iteration = Iteration::Count;
        parallel( &This::_count );
        collect();

        do {
            shared.need_expand = false;
            ring( &This::_por );
            if ( shared.need_expand ) {
                parallel( &This::_count );
                collect();
            }
        } while ( shared.need_expand );
        limits.resize( shareds.size() );
        limits = this->topology().ring( limits, &This::_collectCount );
        this->topology().distribute( limits, &This::_setLimits );
        nodesSize = limits.nodesSize;
        nodes = limits.back().indexEnd - 1;
    }

    void normalize() {
        assert( shared.iteration == Iteration::Count );
        shared.iteration = Iteration::Normalize;
        parallel( &This::_normalize );
    }

    void trackPredecessors() {
        assert( shared.iteration == Iteration::Normalize );
        shared.iteration = Iteration::PrececessorTracking;
        parallel( &This::_trackPredecessors );
    }

    void writeFile() {
        assert( shared.iteration == Iteration::PrececessorTracking );
        shared.iteration = Iteration::WriteFile;

        std::vector< EdgeSpec > initials;
        this->graph().initials( [ this, &initials ]( Node, Node n, Label l ) {
            int64_t id;
            std::tie( std::ignore, id ) = this->topology().ring(
                std::tuple< Node, int64_t >( n, -1 ), &This::_getNodeId );
            assert_neq( -1 /* not found */, id );
            assert_leq( 1, id );
            initials.emplace_back( id, l );
        } );

        assert_eq( nodes, meta().statistics.visited );
        auto creator = compact::preallocate( params.path )
            .nodes( nodes + 1 ) // pseudo-initial
            .edges( meta().statistics.transitions + initials.size() )
            .forward()
            .backward()
            .generator( meta().input.modelType )
            .labelSize( sizeof( Label ) );
        if ( params.saveNodes )
            creator.saveNodes( nodesSize );
        if ( std::is_same< Label, uint64_t >::value )
            creator.uint64Labels();
        if ( std::is_same< Label, toolkit::Probability >::value )
            creator.probability();

        auto compact = creator();

        if ( params.saveNodes )
            compact.nodes.inserter().emplace( 0, []( char *, int64_t ) { } );
        compact.backward.inserter().emplace(
            sizeof( EdgeSpec ) * initials.size(),
                [ &initials ]( char *ptr, int64_t size ) {
                    std::copy( initials.begin(), initials.end(),
                        reinterpret_cast< EdgeSpec * >( ptr ) );
                } );

        auto r = this->topology().ring( true, &This::_writeFile );
        assert( r );
        static_cast< void >( r );
    }

    std::tuple< Node, int64_t > _getNodeId( std::tuple< Node, int64_t > q ) {
        Node n;
        int64_t ix;
        std::tie( n, ix ) = q;
        assert( pool().valid( n ) );
        if ( this->store().knows( n ) ) {
            Vertex v = this->store().fetch( n );
            assert_eq( ix, -1 );
            ix = extension( v ).index;
        }
        return std::make_tuple( n, ix );
    }

    bool _writeFile( bool ok ) {
        assert( ok );

        return  params.saveNodes
            ? _writeFileT< true >()
            : _writeFileT< false >();
    }

    template< bool saveStates >
    bool _writeFileT() {
        compact::Compact compact( params.path, O_RDWR, PROT_READ | PROT_WRITE );
        auto start = limits[ params.ringId ].indexStart;
        auto edgeInserter = compact.backward.inserter( start );
        auto nodeInserter = compact.nodes.inserter( start );

        for ( auto st : this->store() ) {
            assert( extension( st ).iteration == Iteration::PrececessorTracking );
            auto ext = extension( st );
            edgeInserter.emplace( ext.predSize( pool() ),
                [ ext, this ]( char *ptr, int64_t size ) {
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
        }
        return true;
    }

    // after this store must NOT be accessed
    void _cleanup() {
        for ( auto st : this->store() ) {
            this->pool().free( extension( st )._data.predecessors );
//            this->pool().free( st );
        }
    }
};

ALGORITHM_RPC( _Compact );
ALGORITHM_RPC_ID( _Compact, 1, _init );
ALGORITHM_RPC_ID( _Compact, 2, _count );
ALGORITHM_RPC_ID( _Compact, 3, _por );
ALGORITHM_RPC_ID( _Compact, 4, _por_worker );
ALGORITHM_RPC_ID( _Compact, 5, _normalize );
ALGORITHM_RPC_ID( _Compact, 6, _trackPredecessors );
ALGORITHM_RPC_ID( _Compact, 7, _writeFile );
ALGORITHM_RPC_ID( _Compact, 8, _collectCount );
ALGORITHM_RPC_ID( _Compact, 9, _setLimits );
ALGORITHM_RPC_ID( _Compact, 10, _getNodeId );
ALGORITHM_RPC_ID( _Compact, 11, _cleanup );

template< typename Setup >
struct _CompactTranspose : Algorithm, Sequential
{
    std::string path;

    _CompactTranspose( Meta &meta, std::string path ) :
        Algorithm( meta ),
        path( path )
    { }

    void run() {
        progress() << "  transposing graph... \t\t " << std::flush;
        transpose();
        progress() << "done" << std::endl;
    }

    void transpose() {
        compact::Compact compact( path, O_RDWR, PROT_READ | PROT_WRITE );
        compact::transpose< typename _Compact< Setup >::EdgeSpec >(
                compact.backward, compact.forward );
    }
};

template< typename Setup >
struct Compact : Algorithm {

    using Stage1 = _Compact< Setup >;
    using Stage2 = _CompactTranspose< Setup >;

    std::unique_ptr< Stage1 > stage1;
    std::unique_ptr< Stage2 > stage2;

    Compact( Meta m ) :
        Algorithm( m ),
        stage1( new Stage1( m ) ),
        stage2()
    { }

    Compact( Compact &master, int id ) :
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
