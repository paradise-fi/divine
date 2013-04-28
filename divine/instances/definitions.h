// -*- C++ -*- (c) 2012 Petr Rockai <me@mornfall.net>

#include <divine/utility/statistics.h>
#include <divine/algorithm/common.h>
#include <divine/algorithm/por-c3.h>
#include <divine/graph/fairness.h>

#include <divine/generator/dve.h>
#include <divine/generator/llvm.h>
#include <divine/generator/coin.h>
#include <divine/generator/cesmi.h>
#include <divine/generator/timed.h>
#include <divine/generator/dummy.h>

#ifndef DIVINE_INSTANCES_DEFINITIONS
#define DIVINE_INSTANCES_DEFINITIONS

namespace divine {

namespace instantiate {

    template < typename T, T v >
    struct ShowT { };

#define SHOW( TYPE, VAL ) template<> \
    struct ShowT< TYPE, TYPE::VAL > { \
        constexpr const static char* value = #VAL; \
    }

    enum class Algorithm {
        NestedDFS, Owcty, Map, Reachability, Metrics, Simulate, Info, Draw,
        NotSelected
    };

    SHOW( Algorithm, NestedDFS );
    SHOW( Algorithm, Owcty );
    SHOW( Algorithm, Map );
    SHOW( Algorithm, Reachability );
    SHOW( Algorithm, Metrics );
    SHOW( Algorithm, Simulate );
    SHOW( Algorithm, Info );
    SHOW( Algorithm, Draw );
    SHOW( Algorithm, NotSelected );

    enum class Transform {
        None, POR, Fairness,
        NotSelected
    };

    SHOW( Transform, None );
    SHOW( Transform, POR );
    SHOW( Transform, Fairness );
    SHOW( Transform, NotSelected );

    enum class Generator {
        Dve, Coin, LLVM, Timed, CESMI, Dummy,
        NotSelected
    };

    SHOW( Generator, Dve );
    SHOW( Generator, Coin );
    SHOW( Generator, LLVM );
    SHOW( Generator, Timed );
    SHOW( Generator, CESMI );
    SHOW( Generator, Dummy );
    SHOW( Generator, NotSelected );

    enum class Store {
        Partitioned, HashCompacted, NTree, Compressed, Shared,
        NotSelected
    };

    SHOW( Store, Partitioned );
    SHOW( Store, HashCompacted );
    SHOW( Store, NTree );
    SHOW( Store, Shared );
    SHOW( Store, NotSelected );

    enum class Visitor {
        Partitioned, Shared,
        NotSelected
    };

    SHOW( Visitor, Partitioned );
    SHOW( Visitor, Shared );
    SHOW( Visitor, NotSelected );

    enum class Topology {
        Local, Mpi,
        NotSelected
    };

    SHOW( Topology, Local );
    SHOW( Topology, Mpi );
    SHOW( Topology, NotSelected );

    enum class Statistics {
        Enabled, Disabled
    };

    template < Algorithm algo >
    struct SelectAlgorithm {
        static const bool available = false;
    };

#define ALGO_SPEC( ALG ) template <> \
    struct SelectAlgorithm< Algorithm :: ALG > { \
        template < typename Setup > \
        using T = ::divine::algorithm :: ALG < Setup >; \
        static const bool available = true; \
    }

    template < Store store, Visitor visitor >
    struct SelectStore {
        static const bool available = false;
    };
// !! No undef here -> it has to be in algorithms

    template < Visitor visitor >
    struct SelectVisitor {
        static const bool available = false;
    };

#define VISIT_SPEC( VISIT ) template<> \
    struct SelectVisitor< Visitor :: VISIT > { \
        using T = ::divine::visitor :: VISIT; \
        using TableProvider = ::divine::visitor :: VISIT ## Provider; \
        static const bool available = true; \
    }

    VISIT_SPEC( Partitioned );
    VISIT_SPEC( Shared );

#undef VISIT_SPEC

#define STORE_SPEC( ENUM, STORE, VISIT ) template<> \
    struct SelectStore< Store :: ENUM, Visitor :: VISIT > { \
        template < typename Generator, typename Hasher, typename Stat > \
        using T = ::divine::visitor :: STORE < typename SelectVisitor< Visitor :: VISIT > \
            ::TableProvider, Generator, Hasher, Stat >; \
        static const bool available = SelectVisitor< Visitor :: VISIT >::available; \
    }

    STORE_SPEC( Partitioned, DefaultStore, Partitioned );
    STORE_SPEC( Partitioned, DefaultStore, Shared );
#ifdef O_HASH_COMPACTION
    STORE_SPEC( HashCompacted, HcStore, Partitioned );
    STORE_SPEC( HashCompacted, HcStore, Shared );
#endif
#ifdef O_COMPRESSION
    STORE_SPEC( NTree, NTreeStore, Partitioned );
    STORE_SPEC( NTree, NTreeStore, Shared );
#endif

#undef STORE_SPEC

    template < Transform transform >
    struct SelectTransform {
        static const bool available = false;
    };

#define TRANS_SPEC( ENUM, TRANS ) template<> \
    struct SelectTransform< Transform :: ENUM > { \
        template < typename Graph, typename Store, typename StatIgnored > \
        using T = ::divine::graph :: TRANS < Graph, Store >; \
        static const bool available = true; \
    }

    TRANS_SPEC( None, NonPORGraph );

#ifndef O_SMALL
    TRANS_SPEC( Fairness, FairGraph );
    template<>
    struct SelectTransform< Transform::POR > {
        template < typename Graph, typename Store, typename Stat >
        using T = ::divine::algorithm::PORGraph< Graph, Store, Stat >;
        static const bool available = true;
    };
#endif

#undef TRANS_SPEC

    template < Generator generator >
    struct SelectGenerator {
        static const bool available = false;
    };

#define GEN_SPEC( GEN ) template<> \
    struct SelectGenerator< Generator::GEN > { \
        using T = ::divine::generator::GEN; \
        static const bool available = true; \
    }

#ifdef O_DVE
    GEN_SPEC( Dve );
#endif
#if defined( O_COIN ) && !defined( O_SMALL )
    GEN_SPEC( Coin );
#endif
#ifdef O_LLVM
    GEN_SPEC( LLVM );
#endif
#ifdef O_TIMED
    GEN_SPEC( Timed );
#endif
    GEN_SPEC( CESMI );
#ifndef O_SMALL
    GEN_SPEC( Dummy );
#endif

#undef GEN_SPEC

    template < Topology topology >
    struct SelectTopology {
        static const bool available = false;
    };

#define TOPO_SPEC( TOPO ) template<> \
    struct SelectTopology< Topology :: TOPO > { \
        template < typename Transition > \
        struct T { \
            template < typename I > \
            using TT = typename ::divine::Topology< Transition > \
                          ::template TOPO < I >; \
        }; \
        static const bool available = true; \
    }

    TOPO_SPEC( Local );
    TOPO_SPEC( Mpi );

#undef TOPO_SPEC

    template < Statistics statistics >
    struct SelectStatistics {
        using T = ::divine::NoStatistics;
    };

    template<>
    struct SelectStatistics< Statistics::Enabled > {
        using T = ::divine::TrackStatistics;
    };

    template< typename G, typename St >
    using Transition = std::tuple< typename St::Handle, typename G::Node, typename G::Label >;

    template < typename Generator, template < typename, typename, typename > class Transform,
             template< typename, typename, typename > class _Store, typename Hasher,
             typename _Visitor, template< typename > class _Topology,
             typename _Statistics >
    struct Setup {
        using Store = _Store< Generator, Hasher, _Statistics >;
        using Graph = Transform< Generator, Store, _Statistics >;
        using Visitor = _Visitor;
        template < typename I >
        using Topology = typename _Topology< Transition< Graph, Store > >
                            ::template TT< I >;
        using Statistics = _Statistics;
    };

    template < bool, bool, bool, bool, bool, bool, Algorithm algo,
             Generator generator, Transform transform, Store store,
             Visitor visitor, Topology topology, Statistics statistics >
    struct AlgorithmSelector2 {
        algorithm::Algorithm* operator()( Meta& ) {
            return nullptr;
        }
    };

    template < Algorithm algo, Generator generator, Transform transform,
             Store store, Visitor visitor, Topology topology,
             Statistics statistics >
    struct AlgorithmSelector2< true, true, true, true, true, true,
        algo, generator, transform, store, visitor, topology, statistics>
    {
        algorithm::Algorithm* operator()( Meta& meta ) {
#if O_LLVM
            if ( generator == Generator::LLVM
                    && meta.execution.threads > 1 && !::llvm::llvm_is_multithreaded() )
            {
                if ( !::llvm::llvm_start_multithreaded() ) {
                    std::cerr << "FATAL: This binary is linked to single-threaded LLVM." << std::endl
                              << "Multi-threaded LLVM is required for parallel algorithms." << std::endl;
                    return nullptr;
                }
            }
#endif

            return new typename SelectAlgorithm< algo >::template T< Setup<
                    typename SelectGenerator< generator >::T,
                    SelectTransform< transform >::template T,
                    SelectStore< store, visitor >::template T,
                    algorithm::Hasher,
                    typename SelectVisitor< visitor >::T,
                    SelectTopology< topology >::template T,
                    typename SelectStatistics< statistics >::T
                > >( meta );
        }
    };

    constexpr Store fixStore( Algorithm algo, Store store ) {
        return algo == Algorithm::Simulate ? Store::Partitioned : store;
    }

    constexpr Visitor fixVisitor( Algorithm algo, Visitor visitor ) {
        return algo == Algorithm::Simulate ? Visitor::Partitioned : visitor;
    }

    template < Algorithm algo, Generator generator, Transform transform,
             Store store, Visitor visitor, Topology topology,
             Statistics statistics >
    struct AlgorithmSelector {
        algorithm::Algorithm* operator()( Meta& meta ) {
            AlgorithmSelector2<
                    SelectAlgorithm< algo >::available,
                    SelectGenerator< generator >::available,
                    SelectTransform< transform >::available,
                    SelectStore< fixStore( algo, store ), fixVisitor( algo, visitor ) >::available,
                    SelectVisitor< fixVisitor( algo, visitor ) >::available,
                    SelectTopology< topology >::available,
                    algo,
                    generator,
                    transform,
                    fixStore( algo, store ),
                    fixVisitor( algo, visitor ),
                    topology,
                    statistics
                > select;
            return select( meta );
        }
    };


    template < typename T >
    constexpr T ifPerformance( T perf, T noPerf ) {
#ifdef O_PERFORMANCE
        return perf;
#else
        return noPerf;
#endif
    }
}
}

#endif // DIVINE_INSTANCES_DEFINITIONS
