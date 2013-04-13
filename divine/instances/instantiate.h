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

        if ( !SelectAlgorithm< algo >::available )
            std::cerr << "Missing algorithm: "
                << ShowT< Algorithm, algo >::value  << std::endl;
        if ( !SelectGenerator< generator >::available )
            std::cerr << "Missing generator: "
                << ShowT< Generator, generator >::value << std::endl;
        if ( !SelectTransform< transform >::available ) {
            std::cerr << "Missing graf transformation: "
                << ShowT< Transform, transform >::value
                << ", disabling transformations." << std::endl;
            return makeAlgorithm< algo, generator, Transform::None,
                   store, visitor, topology, statistics >( meta );
        }
        if ( !SelectStore< store, visitor >::available ) {
            std::cerr << "Missing store: "
                << ShowT< Store, store >::value
                << ", using Partitioned instead." << std::endl;
            return makeAlgorithm< algo, generator, transform,
                   Store::Partitioned, visitor, topology, statistics >( meta );
        }
        if ( !SelectVisitor< visitor >::available ) {
            std::cerr << "Missing visitor: "
                << ShowT< Visitor, visitor >::value
                << ", using Partitioned instead." << std::endl;
            return makeAlgorithm< algo, generator, transform,
                   store, Visitor::Partitioned, topology, statistics >( meta );
        }
        if ( !SelectTopology< topology >::available )
            std::cerr << "Missing topology: "
                << ShowT< Topology, topology >::value << std::endl;

        AlgorithmSelector< algo, generator, transform, store, visitor,
            topology, statistics > selector;

        return selector( meta );
    }


    template < Algorithm algo, Generator generator, Transform transform,
             Store store, Visitor visitor, Topology topology, Statistics statistics >
    algorithm::Algorithm* selectVisitor( Meta& meta ) {
        if ( meta.algorithm.sharedVisitor )
            return makeAlgorithm< algo, generator, transform, Store::Shared,
                   Visitor::Shared, topology, statistics >( meta );
        else
            return makeAlgorithm< algo, generator, transform, store,
                   Visitor::Partitioned, topology, statistics>( meta );
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

    template < Algorithm algo, Generator generator, Transform transform,
             Store store, Visitor visitor, Topology topology, Statistics statistics >
    algorithm::Algorithm* selectStatistics( Meta& meta ) {
        if ( !meta.output.statistics )
            return selectStore< algo, generator, transform, store, visitor, topology,
                   ifPerformance( Statistics::Disabled, Statistics::Enabled )
                   >( meta );
        return selectStore< algo, generator, transform, store, visitor, topology,
               Statistics::Enabled >( meta );
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
