// -*- C++ -*- (c) 2012 Petr Rockai <me@mornfall.net>

#include <divine/meta.h>
#include <divine/algorithm/common.h>
#include <divine/fairgraph.h>

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

template< template< typename, template< typename > class, typename > class A, typename Graph >
algorithm::Algorithm *makeAlgorithm( Meta &meta )
{
#ifdef O_PERFORMANCE
    if ( !meta.output.statistics )
        return new A< algorithm::NonPORGraph< Graph >, LocalTopology, NoStatistics >( meta, true );
    else
#endif
        return new A< algorithm::NonPORGraph< Graph >, Topology<>::Mpi, Statistics >( meta, true );
}

template< template< typename, template< typename > class, typename > class A, typename Graph >
algorithm::Algorithm *makeAlgorithmPOR( Meta &meta )
{
#ifdef O_PERFORMANCE
    if ( !meta.output.statistics )
        return new A< algorithm::PORGraph< Graph, NoStatistics >, Topology<>::Local, NoStatistics >( meta, true );
    else
#endif
        return new A< algorithm::PORGraph< Graph, Statistics >,
                      Topology< std::pair< typename Graph::Node, typename Graph::Node > >::template Local,
                      Statistics >( meta, true );
}

template< template< typename, template< typename > class, typename > class A >
algorithm::Algorithm *selectGraph( Meta &meta )
{
    if ( wibble::str::endsWith( meta.input.model, ".dve" ) ) {
        meta.input.modelType = "DVE";
#if defined(O_LEGACY) && !defined(O_SMALL)
        if ( meta.algorithm.fairness ) {
            if ( meta.algorithm.por )
                std::cerr << "Fairness with POR is not supported, disabling POR" << std::endl;
            return makeAlgorithm< A, generator::LegacyDve >( meta );
        }
        if ( meta.algorithm.por ) {
            return makeAlgorithmPOR< A, generator::LegacyDve >( meta );
        } else
#endif
        {
#if defined(O_DVE)
            return makeAlgorithm< A, generator::Dve >( meta );
#endif
#if defined(O_LEGACY)
            return makeAlgorithm< A, generator::LegacyDve >( meta );
#endif
        }
#if defined(O_LEGACY) && !defined(O_SMALL)
    } else if ( wibble::str::endsWith( meta.input.model, ".probdve" ) ) {
        meta.input.modelType = "ProbDVE";
        return makeAlgorithm< A, generator::LegacyProbDve >( meta );
#endif
    } else if ( wibble::str::endsWith( meta.input.model, ".compact" ) ) {
        meta.input.modelType = "Compact";
        return makeAlgorithm< A, generator::Compact >( meta );
#ifndef O_SMALL
    } else if ( wibble::str::endsWith( meta.input.model, ".coin" ) ) {
        meta.input.modelType = "CoIn";
        if ( meta.algorithm.por ) {
            return makeAlgorithmPOR< A, generator::Coin >( meta );
        } else {
            return makeAlgorithm< A, generator::Coin >( meta );
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
        return makeAlgorithm< A, generator::LLVM >( meta );
#else
        std::cerr << "FATAL: This binary was built without LLVM support." << std::endl;
        return NULL;
#endif

#if defined(O_LEGACY) && !defined(O_SMALL)
    } else if ( wibble::str::endsWith( meta.input.model, ".b" ) ) {
        meta.input.modelType = "NIPS";
        return makeAlgorithm< A, generator::LegacyBymoc >( meta );
#endif
    } else if ( wibble::str::endsWith( meta.input.model, ".so" ) ) {
        meta.input.modelType = "CESMI";
        return makeAlgorithm< A, generator::CESMI >( meta );
#ifndef O_SMALL
    } else if ( meta.input.dummygen ) {
        meta.input.modelType = "dummy";
        return makeAlgorithm< A, generator::Dummy >( meta );
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
