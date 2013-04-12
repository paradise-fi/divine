// -*- C++ -*- (c) 2012 Petr Rockai <me@mornfall.net>

#include <divine/instances/definitions.h>
#include <divine/utility/meta.h>

#ifndef DIVINE_INSTANCES_INSTANTIATE
#define DIVINE_INSTANCES_INSTANTIATE

namespace divine {

namespace instantiate {

    template < Algorithm algo, Generator generator, Transform transform,
             Store store, Visitor visitor, Topology topology, Statistics statistics >
    algorithm::Algorithm* makeAlgorithm( Meta& meta ) {

template< typename Stats, typename T >
wibble::Unit setupParallel( NotPreferred, T &t )
{
    t.init();
    setupCurses();
    if ( statistics )
        Stats::global().start();
    return wibble::Unit();
}
*/

template< typename G >
using Transition = std::tuple< typename G::Node, typename G::Node, typename G::Label >;

template< template< typename > class T, typename V = visitor::Partitioned >
struct SetupT {
    template< typename X > using Topology = T< X >;
    typedef V Visitor;
};

template< template< typename > class A, typename G, template< typename > class T, typename S,
          typename St >
algorithm::Algorithm *makeAlgorithm( Meta &meta ) {
    struct Setup : SetupT< T > {
        typedef G Graph;
        typedef S Statistics;
        typedef St Store;
    };
    return new A< Setup >( meta );
}

template< template< typename > class A, typename G, template< typename > class T, typename S >
algorithm::Algorithm *makeAlgorithm( Meta &meta ) {
    if ( meta.algorithm.hashCompaction )
#if O_HASH_COMPACTION
        return makeAlgorithm< A, G, T, S, visitor::HcStore< G, algorithm::Hasher, S > >( meta );
    else
#else
        std::cerr << "Hash compaction is disabled, running without compaction (consider recompiling with -DO_HASH_COMPACTION)" << std::endl;
#endif
        return makeAlgorithm< A, G, T, S, visitor::PartitionedStore< G, algorithm::Hasher, S > >( meta );
}

template< template< typename > class A, typename G, template< typename > class T >
algorithm::Algorithm *makeAlgorithm( Meta &meta )
{
#ifdef O_PERFORMANCE
    if ( !meta.output.statistics )
        return makeAlgorithm< A, G, T, divine::NoStatistics >( meta );
    else
#endif
        return makeAlgorithm< A, G, T, divine::Statistics >( meta );
}

template< template< typename > class A, typename G >
algorithm::Algorithm *makeAlgorithm( Meta &meta )
{
#ifdef O_PERFORMANCE
    if ( meta.execution.nodes == 1 )
        return makeAlgorithm< A, G, Topology< Transition< G > >::template Local >( meta );
    else
#endif
        return makeAlgorithm< A, G, Topology< Transition< G > >::template Mpi >( meta );
}

    template < Algorithm algo, Generator generator, Transform transform,
             Store store, Visitor visitor, Topology topology, Statistics statistics >
    algorithm::Algorithm* selectStore( Meta& meta ) {
        if ( meta.algorithm.hashCompaction )
            return selectVisitor< algo, generator, transform, Store::HashCompacted,
                   visitor, topology, statistics >( meta );
        if ( meta.algorithm.compression == meta::Algorithm::C_Tree )
            return selectVisitor< algo, generator, transform, Store::Compressed,
                   visitor, topology, statistics >( meta );
        return selectVisitor< algo, generator, transform, Store::Partitioned,
               visitor, topology, statistics >( meta );
    }

template< template< typename > class A, typename Graph >
algorithm::Algorithm *makeAlgorithmPOR( Meta &meta )
{
#ifdef O_PERFORMANCE
    if ( !meta.output.statistics )
        return makeAlgorithm< A, algorithm::PORGraph< Graph, divine::NoStatistics > >( meta );
    else
#endif
        return makeAlgorithm< A, algorithm::PORGraph< Graph, divine::Statistics > >( meta );
}

    template < Algorithm algo, Generator generator, Transform transform,
             Store store, Visitor visitor, Topology topology, Statistics statistics >
    algorithm::Algorithm* selectTopology( Meta& meta ) {
        if ( meta.execution.nodes == 1 )
            return selectStatistics< algo, generator, transform, store, visitor,
                   ifPerformance( Topology::Local, Topology::Mpi ), statistics
                   >( meta );
        return selectTopology< algo, generator, transform, store,
               visitor, Topology::Mpi, statistics >( meta );
    }

    template < Algorithm algo, Generator generator, Transform transform,
             Store store, Visitor visitor, Topology topology, Statistics statistics >
    algorithm::Algorithm* selectTransform( Meta& meta ) {
        static_assert( generator != Generator::NotSelected,
                "selectGenerator needs to be called before selectTransform" );
        const bool por = meta.algorithm.reduce.count( graph::R_POR );
        if ( meta.algorithm.fairness ) {
            if ( por )
                std::cerr << "Fairness with POR is not supported, disabling POR"
                    << std::endl;
            return selectTopology< algo, generator,
                   generator == Generator::Dve
                       ? Transform::Fairness
                       : Transform::None,
                   store, visitor, topology, statistics >( meta );
        }
        if ( meta.algorithm.reduce.count( graph::R_POR ) )
            return selectTopology< algo, generator,
                   generator == Generator::Dve || generator == Generator::Coin
                       ? Transform::POR
                       : Transform::None,
                   store, visitor, topology, statistics >( meta );

        return selectTopology< algo, generator, Transform::None, store,
                visitor, topology, statistics >( meta );
    }

    template < Algorithm algo, Generator generator, Transform transform,
             Store store, Visitor visitor, Topology topology, Statistics statistics >
    algorithm::Algorithm* selectGenerator( Meta& meta ) {
        if ( wibble::str::endsWith( meta.input.model, ".dve" ) ) {
            meta.input.modelType = "DVE";
            return selectTransform< algo, Generator::Dve, transform, store, visitor,
                topology, statistics >( meta );
        } else if ( wibble::str::endsWith( meta.input.model, ".coin" ) ) {
            meta.input.modelType = "CoIn";
            return selectTransform< algo, Generator::Coin, transform, store,
                   visitor, topology, statistics >( meta );
        } else if ( wibble::str::endsWith( meta.input.model, ".bc" ) ) {
            meta.input.modelType = "LLVM";
            return selectTransform< algo, Generator::LLVM, transform, store,
                   visitor, topology, statistics >( meta );
        } else if ( wibble::str::endsWith( meta.input.model, ".xml" ) ) {
            meta.input.modelType = "Timed";
            return selectTransform< algo, Generator::Timed, transform, store,
                   visitor, topology, statistics >( meta );
        } else if ( wibble::str::endsWith( meta.input.model, generator::cesmi_ext ) ) {
            meta.input.modelType = "CESMI";
            return selectTransform< algo, Generator::CESMI, transform, store,
                   visitor, topology, statistics >( meta );
        } else if ( meta.input.dummygen ) {
            meta.input.modelType = "dummy";
            return selectTransform< algo, Generator::Dummy, transform, store,
                   visitor, topology, statistics >( meta );
        }
        std::cerr << "FATAL: Unknown input file extension." << std::endl;
        std::cerr << "FATAL: Internal error choosing generator." << std::endl;
        return nullptr;
    }

    algorithm::Algorithm *selectOWCTY( Meta &m );
    algorithm::Algorithm *selectMAP( Meta &m );
    algorithm::Algorithm *selectNDFS( Meta &m );
    algorithm::Algorithm *selectMetrics( Meta &m );
    algorithm::Algorithm *selectReachability( Meta &m );
    algorithm::Algorithm *selectProbabilistic( Meta &m );
    algorithm::Algorithm *selectCompact( Meta &m );
    algorithm::Algorithm *selectSimulate( Meta &m );
}

algorithm::Algorithm *select( Meta &m );

}

#endif // DIVINE_INSTANCES_INSTANTIATE
