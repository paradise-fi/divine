// -*- C++ -*- (c) 2012 Petr Rockai <me@mornfall.net>

#include <divine/utility/meta.h>
#include <divine/algorithm/common.h>
#include <divine/algorithm/por-c3.h>
#include <divine/graph/fairness.h>

#include <divine/generator/legacy.h>
#include <divine/generator/compact.h>

#include <divine/generator/dve.h>
#include <divine/generator/llvm.h>
#include <divine/generator/coin.h>
#include <divine/generator/cesmi.h>
#include <divine/generator/dummy.h>

#ifndef DIVINE_SELECT_H
#define DIVINE_SELECT_H

namespace divine {

/*
template< typename Stats, typename T >
typename T::IsDomainWorker setupParallel( Preferred, T &t )
{
    t.domain().mpi.init();
    t.init( &t.domain(), m.execution.diskFifo );

    if ( m.output.curses && t.domain().mpi.master() )
        Output::_output = makeCurses();

    Stats::global().useDomain( t.domain() );
    if ( statistics )
        Stats::global().start();
    t.domain().mpi.start();
    report.mpiInfo( t.domain().mpi );
    return wibble::Unit();
}

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

template< template< typename > class T >
struct SetupT {
    template< typename X > using Topology = T< X >;
};

template< template< typename > class A, typename G, template< typename > class T, typename S,
          typename St >
algorithm::Algorithm *makeAlgorithm( Meta &meta ) {
    struct Setup : SetupT< T > {
        typedef G Graph;
        typedef S Statistics;
        typedef St Store;
    };
    return new A< Setup >( meta, true );
}

template< template< typename > class A, typename G, template< typename > class T, typename S >
algorithm::Algorithm *makeAlgorithm( Meta &meta ) {
    if ( meta.algorithm.hashCompaction )
        abort(); // return makeAlgorithm< A, G, T, S, visitor::HcStore< G, algorithm::Hasher, S > >( meta );
    else
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

template< template< typename > class A, typename Graph >
algorithm::Algorithm *makeAlgorithmN( Meta &meta )
{
    return makeAlgorithm< A, graph::NonPORGraph< Graph > >( meta );
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

template< template< typename > class A >
algorithm::Algorithm *selectGraph( Meta &meta )
{
    if ( wibble::str::endsWith( meta.input.model, ".dve" ) ) {
        meta.input.modelType = "DVE";
#if defined(O_LEGACY) && !defined(O_SMALL)
        if ( meta.algorithm.fairness ) {
            if ( meta.algorithm.por )
                std::cerr << "Fairness with POR is not supported, disabling POR" << std::endl;
            return makeAlgorithmN< A, generator::LegacyDve >( meta );
        }
        if ( meta.algorithm.por ) {
            return makeAlgorithmPOR< A, generator::LegacyDve >( meta );
        } else if ( meta.algorithm.labels || meta.algorithm.traceLabels ) {
            return makeAlgorithmN< A, generator::LegacyDve >( meta );
        } else
#endif
        {
#if defined(O_DVE)
            return makeAlgorithmN< A, generator::Dve >( meta );
#endif
#if defined(O_LEGACY)
            return makeAlgorithmN< A, generator::LegacyDve >( meta );
#endif
        }
#if defined(O_LEGACY) && !defined(O_SMALL)
    } else if ( wibble::str::endsWith( meta.input.model, ".probdve" ) ) {
        meta.input.modelType = "ProbDVE";
        return makeAlgorithmN< A, generator::LegacyProbDve >( meta );
#endif
    } else if ( wibble::str::endsWith( meta.input.model, ".compact" ) ) {
        meta.input.modelType = "Compact";
        return makeAlgorithmN< A, generator::Compact >( meta );
#if defined(O_COIN) && !defined(O_SMALL)
    } else if ( wibble::str::endsWith( meta.input.model, ".coin" ) ) {
        meta.input.modelType = "CoIn";
        if ( meta.algorithm.por ) {
            return makeAlgorithmPOR< A, generator::Coin >( meta );
        } else {
            return makeAlgorithmN< A, generator::Coin >( meta );
        }
#endif
    } else if ( meta.algorithm.por ) {
        std::cerr << "FATAL: Partial order reduction is not supported for this input type."
                  << std::endl;
        return NULL;
    } else if ( wibble::str::endsWith( meta.input.model, ".bc" ) ) {
        meta.input.modelType = "LLVM";
#ifdef O_LLVM
        if ( meta.execution.threads > 1 && !::llvm::llvm_start_multithreaded() ) {
            std::cerr << "FATAL: This binary is linked to single-threaded LLVM." << std::endl
                      << "Multi-threaded LLVM is required for parallel algorithms." << std::endl;
            return NULL;
        }
        return makeAlgorithmN< A, generator::LLVM >( meta );
#else
        std::cerr << "FATAL: This binary was built without LLVM support." << std::endl;
        return NULL;
#endif

#if defined(O_LEGACY) && !defined(O_SMALL)
    } else if ( wibble::str::endsWith( meta.input.model, ".b" ) ) {
        meta.input.modelType = "NIPS";
        return makeAlgorithmN< A, generator::LegacyBymoc >( meta );
#endif
    } else if ( wibble::str::endsWith( meta.input.model, ".so" ) ) {
        meta.input.modelType = "CESMI";
        return makeAlgorithmN< A, generator::CESMI >( meta );
#ifndef O_SMALL
    } else if ( meta.input.dummygen ) {
        meta.input.modelType = "dummy";
        return makeAlgorithmN< A, generator::Dummy >( meta );
#endif
    } else {
        std::cerr << "FATAL: Unknown input file extension." << std::endl;
        return NULL;
    }

    std::cerr << "FATAL: Internal error choosing generator." << std::endl;
    return NULL;
}

algorithm::Algorithm *selectOWCTY( Meta &m );
algorithm::Algorithm *selectMAP( Meta &m );
algorithm::Algorithm *selectNDFS( Meta &m );
algorithm::Algorithm *selectMetrics( Meta &m );
algorithm::Algorithm *selectReachability( Meta &m );
algorithm::Algorithm *selectProbabilistic( Meta &m );
algorithm::Algorithm *selectCompact( Meta &m );

algorithm::Algorithm *select( Meta &m );

}

#endif
