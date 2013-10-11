// -*- C++ -*- (c) 2013 Vladimír Štill <xstill@fi.muni.cz>

/* Automatic instantiation definitions -- see divine/instances/README */

#include <divine/utility/meta.h>
#include <wibble/sfinae.h>
#include <divine/generator/cesmi.h>
#include <divine/toolkit/typelist.h>

#include <divine/llvm/support.h>
#include <divine/explicit/header.h>

#ifndef DIVINE_INSTANCES_DEFINITIONS
#define DIVINE_INSTANCES_DEFINITIONS

namespace divine {
namespace instantiate {

struct NotSelected { };

/* for marking components as missing based on cmake */
struct _Missing { using Missing = wibble::Unit; };

/* base for isntantiation errors if no component is available */
struct InstantiationError { };

using Any = True;

namespace algorithm {
    struct IsAlgorithmT { };

    struct NoAlgorithmErr : public InstantiationError {
        static std::string instantiationError( Meta & ) {
            return "Selected algorithm is not available";
        }
    };

#define ALGORITHM_NS( ALGO, META_ID, NAME, HEADER, NS ) struct ALGO { \
        using IsAlgorithm = IsAlgorithmT; \
        static constexpr const char *header = HEADER; \
        static constexpr const char *symbol = "template< typename Setup >\n" \
                        "using _Algorithm = " #NS "::" #ALGO "< Setup >;\n"; \
        static constexpr const char *key = #ALGO; \
        static constexpr const char *name = NAME; \
        using SupportedBy = Any; \
        static bool select( Meta &meta ) { \
            return meta.algorithm.algorithm == meta::Algorithm::Type:: META_ID; \
        } \
        static void postSelect( Meta &meta ) { \
            meta.algorithm.name = name; \
        } \
    }

#define ALGORITHM( ALGO, META_ID, NAME, HEADER ) \
    ALGORITHM_NS( ALGO, META_ID, NAME, HEADER, ::divine::algorithm )

    ALGORITHM( NestedDFS,    Ndfs,         "Nested DFS",   "divine/algorithm/nested-dfs.h" );
    ALGORITHM( Owcty,        Owcty,        "OWCTY",        "divine/algorithm/owcty.h" );
    ALGORITHM( Map,          Map,          "MAP",          "divine/algorithm/map.h" );
    ALGORITHM( Reachability, Reachability, "Reachability", "divine/algorithm/reachability.h" );
    ALGORITHM( Metrics,      Metrics,      "Metrics",      "divine/algorithm/metrics.h" );
    ALGORITHM( Simulate,     Simulate,     "Simulate",     "divine/algorithm/simulate.h" );
#if O_EXPLICIT
    ALGORITHM( GenExplicit,      GenExplicit,      "GenExplicit",      "divine/algorithm/genexplicit.h" );
#else
    using GenExplicit = _Missing;
#endif
    ALGORITHM_NS( Draw,      Draw,         "Draw",         "tools/draw.h", ::divine );
    ALGORITHM_NS( Info,      Info,         "Info",         "tools/info.h", ::divine );


    using Algorithms = TypeList< NestedDFS, Owcty, Map, Reachability,
              Metrics, Simulate, GenExplicit, Draw, Info,
              NoAlgorithmErr
          >;
#undef ALGORITHM
#undef ALGORITHM_NS
}

namespace generator {
    struct IsGeneratorT { };

    struct NoGeneratorErr : public InstantiationError {
        static std::string instantiationError( Meta & ) {
            return "Unknown input file extension.";
        }
    };

#define GENERATOR( GEN, EXTENSION, NAME, SUPPORTED_BY, HEADER ) struct GEN { \
        using IsGenerator = IsGeneratorT; \
        static constexpr const char *header = HEADER; \
        static constexpr const char *symbol = "using _Generator = ::divine::generator::" #GEN ";\n"; \
        static constexpr const char *key = #GEN; \
        static constexpr const char *name = NAME; \
        static constexpr const char *extension = EXTENSION; \
        using SupportedBy = SUPPORTED_BY; \
        static bool select( Meta &meta ) { \
            return wibble::str::endsWith( meta.input.model, extension ); \
        } \
        \
        static void postSelect( Meta &meta ) { \
            meta.input.modelType = name; \
        } \
    }

#ifdef O_DVE
    GENERATOR( Dve, ".dve", "DVE", Any, "divine/generator/dve.h" );
#else
    using Dve = _Missing;
#endif

#if defined( O_COIN ) && !defined( O_SMALL )
    GENERATOR( Coin, ".coin", "CoIn", Any, "divine/generator/coin.h" );
#else
    using Coin = _Missing;
#endif

#ifdef O_TIMED
    GENERATOR( Timed, ".xml", "Timed", Any, "divine/generator/timed.h" );
#else
    using Timed = _Missing;
#endif

    GENERATOR( CESMI, ::divine::generator::cesmi_ext, "CESMI", Any,
            "divine/generator/cesmi.h" );

#ifdef O_LLVM
    namespace intern {
        GENERATOR( LLVM, ".bc", "LLVM", Any, "divine/generator/llvm.h" );
        GENERATOR( ProbabilisticLLVM, ".bc", "Probabilistic LLVM", Any, "divine/generator/llvm.h" );
        struct LLVMInit {
            void init( Meta &meta ) {
                if ( meta.execution.threads > 1 && !llvm::initMultithreaded() )
                    std::cerr << "FATAL: This binary is linked to single-threaded LLVM." << std::endl
                              << "Multi-threaded LLVM is required for parallel algorithms." << std::endl;
                    assert_unreachable( "LLVM error" );
            }
        };
    }

    struct LLVM : intern::LLVM, intern::LLVMInit {
        static bool select( Meta &meta ) {
            return intern::LLVM::select( meta ) && !meta.input.probabilistic;
        }
    };
    struct ProbabilisticLLVM : intern::ProbabilisticLLVM, intern::LLVMInit {
        static bool select( Meta &meta ) {
            return intern::LLVM::select( meta ) && meta.input.probabilistic;
        }
    };

#else
    using LLVM = _Missing;
    using ProbabilisticLLVM = _Missing;
#endif

#if O_EXPLICIT
    namespace intern {
        GENERATOR( Explicit, dess::extension, "Explicit",
                Not< algorithm::GenExplicit >, "divine/generator/explicit.h" );
        GENERATOR( ProbabilisticExplicit, dess::extension, "Probabilistic explicit",
                Not< algorithm::GenExplicit >, "divine/generator/explicit.h" );
    }
    struct Explicit : public intern::Explicit {
        static bool select( Meta &meta ) {
            return intern::Explicit::select( meta ) &&
                dess::Header::fromFile( meta.input.model ).labelSize == 0;
        }
    };
    struct ProbabilisticExplicit : public intern::ProbabilisticExplicit {
        static bool select( Meta &meta ) {
            return intern::ProbabilisticExplicit::select( meta )
                && dess::Header::fromFile( meta.input.model )
                    .capabilities.has( dess::Capability::Probability );
        }
    };
#else
    using Explicit = _Missing;
    using ProbabilisticExplicit = _Missing;
#endif

#ifndef O_SMALL
    namespace intern {
        GENERATOR( Dummy, nullptr, "Dummy", Any, "divine/generator/dummy.h" );
    }
    struct Dummy : public intern::Dummy {
        static bool select( Meta &meta ) {
            return meta.input.dummygen;
        }
    };
#else
    using Dummy = _Missing;
#endif

    using Generators = TypeList< Dve, Coin, LLVM, ProbabilisticLLVM, Timed, CESMI,
                                 ProbabilisticExplicit, Explicit, Dummy, NoGeneratorErr >;

#undef GENERATOR
}

namespace transform {
    struct IsTransformT { };

#define TRANSFORM( TRANS, SYMBOL, SELECTOR, SUPPORTED_BY, HEADER ) struct TRANS { \
        using IsTransform = IsTransformT; \
        static constexpr const char *header = HEADER; \
        static constexpr const char *symbol = "template< typename Graph, " \
                "typename Store, typename Stat >\n" \
                "using _Transform = " SYMBOL ";\n"; \
        static constexpr const char *key = #TRANS; \
        using SupportedBy = SUPPORTED_BY; \
        static bool select( Meta &meta ) { \
            return SELECTOR; \
            static_cast< void >( meta ); \
        } \
    }

    TRANSFORM( None, "::divine::graph::NonPORGraph< Graph, Store >", true,
            Any, "divine/graph/por.h" );
#ifndef O_SMALL
    using ForReductions = And< generator::Dve, Not< algorithm::Info > >;
    TRANSFORM( Fairness, "::divine::graph::FairGraph< Graph, Store >",
            meta.algorithm.fairness, ForReductions, "divine/graph/fairness.h" );
    TRANSFORM( POR, "::divine::algorithm::PORGraph< Graph, Store, Stat >",
            meta.algorithm.reduce.count( graph::R_POR ), ForReductions,
            "divine/algorithm/por-c3.h" );
#else
    using Fairness = _Missing;
    using POR = _Missing;
#endif
#undef TRANSFORM

    using Transforms = TypeList< POR, Fairness, None >;
}

namespace visitor {
    struct IsVisitorT { };

#define VISITOR( VISIT, SELECTOR, SUPPORTED_BY ) struct VISIT { \
        using IsVisitor = IsVisitorT; \
        static constexpr const char *symbol = \
                    "using _Visitor = ::divine::visitor::" #VISIT ";\n" \
                    "using _TableProvider = ::divine::visitor::" #VISIT "Provider;\n"; \
        static constexpr const char *key = #VISIT; \
        using SupportedBy = SUPPORTED_BY; \
        static bool select( Meta &meta ) { \
            return SELECTOR; \
            static_cast< void >( meta ); \
        } \
    }

    VISITOR( Partitioned, true, Any );
    using ForShared = Not< Or< algorithm::Simulate, algorithm::GenExplicit, algorithm::Info > >;
    VISITOR( Shared, meta.algorithm.sharedVisitor, ForShared );
#undef VISITOR

    using Visitors = TypeList< Shared, Partitioned >;
}

namespace store {
    struct IsStoreT { };

#define STORE( STOR, SELECTOR, SUPPORTED_BY ) struct STOR { \
        using IsStore = IsStoreT; \
        static constexpr const char *symbol = \
            "template < typename Provider, typename Generator, typename Hasher, typename Stat >\n" \
            "using _Store = ::divine::visitor::" #STOR "< Provider, Generator, Hasher, Stat >;\n"; \
        static constexpr const char *key = #STOR; \
        using SupportedBy = SUPPORTED_BY; \
        static bool select( Meta &meta ) { \
            return SELECTOR; \
            static_cast< void >( meta ); \
        } \
    }

using ForCompressions = Not< Or< algorithm::Info, algorithm::Simulate > >;
#ifdef O_COMPRESSION
    STORE( NTreeStore, meta.algorithm.compression == meta::Algorithm::Compression::Tree,
            ForCompressions );
#else
    using NTreeStore = _Missing;
#endif

#ifdef O_HASH_COMPACTION
    STORE( HcStore, meta.algorithm.hashCompaction, ForCompressions );
#else
    using HcStore = _Missing;
#endif

    STORE( DefaultStore, true, Any );
#undef STORE

    using Stores = TypeList< NTreeStore, HcStore, DefaultStore >;
}

namespace topology {
    struct IsTopologyT { };

#define TOPOLOGY( TOPO, SELECTOR, SUPPORTED_BY ) struct TOPO { \
        using IsTopology = IsTopologyT; \
        static constexpr const char *symbol = \
            "template< typename Transition >\n" \
            "struct _Topology {\n" \
            "    template< typename I >\n" \
            "    using T = typename ::divine::Topology< Transition >\n" \
            "                  ::template " #TOPO "< I >;\n" \
            "};\n"; \
        static constexpr const char *key = #TOPO; \
        using SupportedBy = SUPPORTED_BY; \
        static bool select( Meta &meta ) { \
            return SELECTOR; \
            static_cast< void >( meta ); \
        } \
    }

#if !defined( O_PERFORMANCE ) || defined( O_MPI )
#ifndef O_PERFORMANCE
    using ForMpi = Any; // as we need NDFS without O_PERFORMANCE
#else
    using ForMpi = Not< Or< algorithm::NestedDFS, algorithm::Info, visitor::Shared > >;
#endif

    TOPOLOGY( Mpi, meta.execution.nodes > 1, ForMpi );
#else
    using Mpi = _Missing;
#endif

#ifdef O_PERFORMANCE
    TOPOLOGY( Local, true, Any );
#else
    using Local = _Missing;
#endif

#undef TOPOLOGY

    using Topologies = TypeList< Mpi, Local >;
}

namespace statistics {
    struct IsStatisticsT { };

#define STATISTICS( STAT, SELECTOR, SUPPORTED_BY ) struct STAT { \
        using IsStatistics = IsStatisticsT; \
        static constexpr const char *symbol = "using _Statistics = ::divine::" #STAT ";\n"; \
        static constexpr const char *key = #STAT; \
        static constexpr const char *header = "divine/utility/statistics.h"; \
        using SupportedBy = SUPPORTED_BY; \
        static bool select( Meta &meta ) { \
            return SELECTOR; \
            static_cast< void >( meta ); \
        } \
    }

    STATISTICS( TrackStatistics, meta.output.statistics, Any );
#ifdef O_PERFORMANCE
    using ForNoStat = Not< Or< algorithm::Info, algorithm::Simulate > >;
    STATISTICS( NoStatistics, true, ForNoStat );
#else
    using NoStatistics = _Missing;
#endif
#undef STATISTICS

    using Statistics = TypeList< TrackStatistics, NoStatistics >;
}

template< typename Graph, typename Store >
using Transition = std::tuple< typename Store::Vertex, typename Graph::Node, typename Graph::Label >;

using Instantiate = TypeList<
              algorithm::Algorithms,
              generator::Generators,
              transform::Transforms,
              visitor::Visitors,
              store::Stores,
              topology::Topologies,
              statistics::Statistics
          >;

}
}

#endif // DIVINE_INSTANCES_DEFINITIONS
