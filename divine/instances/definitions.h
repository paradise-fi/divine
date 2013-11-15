// -*- C++ -*- (c) 2013 Vladimír Štill <xstill@fi.muni.cz>

/* Automatic instantiation definitions -- see divine/instances/README */

#include <divine/utility/meta.h>
#include <wibble/sfinae.h>
#include <divine/generator/cesmi.h>
#include <divine/toolkit/typelist.h>
#include <array>

#include <divine/llvm/support.h>
#include <divine/explicit/header.h>
#include <divine/instances/atom.h>

#ifndef DIVINE_INSTANCES_DEFINITIONS
#define DIVINE_INSTANCES_DEFINITIONS

namespace divine {
namespace instantiate {

enum SelectOption { SE_WarnOther = 0x1, SE_LastDefault = 0x2, SE_WarnUnavailable = 0x4 };

template< SelectOption _opts, typename... Ts >
struct _Select {
    using List = TypeList< Ts... >;
    constexpr static SelectOption opts = _opts;
    using WarnOtherAvailable = _Select< SelectOption( SE_WarnOther | _opts ), Ts... >;
    using LastAvailableIsDefault = _Select< SelectOption( SE_LastDefault | _opts ), Ts... >;
    using WarnUnavailable = _Select< SelectOption( SE_WarnUnavailable | _opts ), Ts... >;
};

template< typename... Ts >
struct Select : public _Select< SelectOption( 0 ), Ts... > { };

struct Traits {
    /* this is kind of boilerplate code for accessing compile time macro definitions
     * at runtime
     *
     * no bitfiels -- pointers to members will be taken
     */
    bool performance;
    bool small;
    bool dve;
    bool llvm;
    bool timed;
    bool mpi;
    bool coin;
    bool cesmi;
    bool dess; //explicit
    bool ntree;
    bool hc;
    bool pools;
    bool jit;

    struct Get {
        Get( bool Traits::* p ) :
            fn( [ p ]( const Traits &tr ) -> bool { return tr.*p; } )
        { }
        Get( bool (Traits::*f)() const ) :
            fn( [ f ]( const Traits &tr ) -> bool { return (tr.*f)(); } )
        { }

        bool operator()( const Traits &tr ) { return fn( tr ); }

        Get operator!() const {
            auto fn_ = fn;
            return Get( [ fn_ ]( const Traits &tr ) { return !fn_( tr ); } );
        }

        friend Get operator&&( const Get &a, const Get &b ) {
            auto fa = a.fn;
            auto fb = b.fn;
            return Get( [ fa, fb ]( const Traits &tr ) { return fa( tr ) && fb( tr ); } );
        }

        friend Get operator||( const Get &a, const Get &b ) {
            auto fa = a.fn;
            auto fb = b.fn;
            return Get( [ fa, fb ]( const Traits &tr ) { return fa( tr ) || fb( tr ); } );
        }

      private:
        std::function< bool( const Traits & ) > fn;

        Get( std::function< bool( const Traits & ) > fn ) : fn( fn ) { }
    };

    bool always() const { return true; }

    friend Traits operator|( const Traits &a, const Traits &b ) {
        using BoolArray = std::array< bool, sizeof( Traits ) / sizeof( bool ) >;

        union U {
            BoolArray arr;
            Traits tr;
            U() : tr() { }
            U( const Traits &tr ) : tr( tr ) { }
        };

        U out;
        U au( a ), bu( b );
        for ( int i = 0; i < int( out.arr.size() ); ++i )
            out.arr[ i ] = au.arr[ i ] | bu.arr[ i ];
        return out.tr;
    }

    Traits() { std::memset( this, 0, sizeof( Traits ) ); }

    static Traits available() { return compiled(); }

    static Traits compiled() {
        Traits t;
#ifdef O_PERFORMANCE
        t.performance = true;
#endif
#ifdef O_SMALL
        t.small = true;
#endif
#ifdef O_DVE
        t.dve = true;
#endif
#ifdef O_LLVM
        t.llvm = true;
#endif
#ifdef O_TIMED
        t.timed = true;
#endif
#ifdef O_MPI
        t.mpi = true;
#endif
#ifdef O_COIN
        t.coin = true;
#endif
#ifdef O_CESMI
        t.cesmi = true;
#endif
#ifdef O_EXPLICIT
        t.dess = true;
#endif
#ifdef O_COMPRESSION
        t.ntree = true;
#endif
#ifdef O_HASH_COMPACTION
        t.hc = true;
#endif
#ifdef O_POOLS
        t.pools = true;
#endif
#ifdef O_JIT
        t.jit = true;
#endif
        return t;
    }
};

using Tr = Traits::Get;

#define TRAIT( x ) bool trait( const Traits &tr ) const override { return Traits::Get( x )( tr ); }
#define ATOM( x ) Atom atom() const override { return Atom( x() ); }

struct NotSelected { };

/* for marking components as missing based on cmake */
struct _Missing { using Missing = wibble::Unit; };

using Any = True;

struct Selectable {
    virtual bool select( const Meta &meta ) const = 0;
    virtual void postSelect( Meta &meta ) const { }
    virtual void init( Meta &meta ) const { }
    virtual bool trait( const Traits &tr ) const = 0;
    virtual Atom atom() const = 0;
};

namespace algorithm {
    struct IsAlgorithmT { };

#define ALGORITHM_NS( ALGO, META_ID, NAME, HEADER, NS, TR ) struct ALGO : virtual Selectable { \
        using IsAlgorithm = IsAlgorithmT; \
        static constexpr const char *header = HEADER; \
        static constexpr const char *symbol = "template< typename Setup >\n" \
                        "using _Algorithm = " #NS "::" #ALGO "< Setup >;\n"; \
        static constexpr const char *key = #ALGO; \
        static constexpr const char *name = NAME; \
        using SupportedBy = Any; \
        bool select( const Meta &meta ) const override { \
            return meta.algorithm.algorithm == meta::Algorithm::Type:: META_ID; \
        } \
        void postSelect( Meta &meta ) const override { \
            meta.algorithm.name = name; \
        } \
        TRAIT( TR ) \
        ATOM( ALGO ) \
    }

#define ALGORITHM( ALGO, META_ID, NAME, HEADER, TR ) \
    ALGORITHM_NS( ALGO, META_ID, NAME, HEADER, ::divine::algorithm, TR )

    ALGORITHM( NestedDFS,    Ndfs,         "Nested DFS",   "divine/algorithm/nested-dfs.h", &Traits::always );
    ALGORITHM( Owcty,        Owcty,        "OWCTY",        "divine/algorithm/owcty.h", &Traits::always );
    ALGORITHM( Map,          Map,          "MAP",          "divine/algorithm/map.h", &Traits::always );
    ALGORITHM( Reachability, Reachability, "Reachability", "divine/algorithm/reachability.h", &Traits::always );
    ALGORITHM( Metrics,      Metrics,      "Metrics",      "divine/algorithm/metrics.h", &Traits::always );
    ALGORITHM( Simulate,     Simulate,     "Simulate",     "divine/algorithm/simulate.h", &Traits::always );
    ALGORITHM_NS( Draw,      Draw,         "Draw",         "tools/draw.h", ::divine, &Traits::always );
    ALGORITHM_NS( Info,      Info,         "Info",         "tools/info.h", ::divine, &Traits::always );
    ALGORITHM( GenExplicit,      GenExplicit,      "GenExplicit",      "divine/algorithm/genexplicit.h", &Traits::dess );


    using Algorithms = Select< NestedDFS, Owcty, Map, Reachability,
              Metrics, Simulate, GenExplicit, Draw, Info >::WarnUnavailable;
#undef ALGORITHM
#undef ALGORITHM_NS
}

namespace generator {
    struct IsGeneratorT { };

#define GENERATOR( GEN, EXTENSION, NAME, SUPPORTED_BY, HEADER, TR ) struct GEN : virtual Selectable { \
        using IsGenerator = IsGeneratorT; \
        static constexpr const char *header = HEADER; \
        static constexpr const char *symbol = "using _Generator = ::divine::generator::" #GEN ";\n"; \
        static constexpr const char *key = #GEN; \
        static constexpr const char *name = NAME; \
        static constexpr const char *extension = EXTENSION; \
        using SupportedBy = SUPPORTED_BY; \
        bool select( const Meta &meta ) const override { \
            return wibble::str::endsWith( meta.input.model, extension ); \
        } \
        \
        void postSelect( Meta &meta ) const override { \
            meta.input.modelType = name; \
        } \
        TRAIT( TR ) \
        ATOM( GEN ) \
    }

    GENERATOR( Dve, ".dve", "DVE", Any, "divine/generator/dve.h", &Traits::dve );
    GENERATOR( Coin, ".coin", "CoIn", Any, "divine/generator/coin.h", &Traits::coin );
    GENERATOR( Timed, ".xml", "Timed", Any, "divine/generator/timed.h", &Traits::timed );
    GENERATOR( CESMI, ::divine::generator::cesmi_ext, "CESMI", Any,
            "divine/generator/cesmi.h", &Traits::cesmi );

    namespace intern {
        GENERATOR( LLVM, ".bc", "LLVM", Any, "divine/generator/llvm.h", &Traits::llvm );
        GENERATOR( ProbabilisticLLVM, ".bc", "Probabilistic LLVM", Any,
                "divine/generator/llvm.h", &Traits::llvm );

        struct LLVMInit : virtual Selectable {
            void init( Meta &meta ) const override {
                if ( meta.execution.threads > 1 && !llvm::initMultithreaded() ) {
                    std::cerr << "FATAL: This binary is linked to single-threaded LLVM." << std::endl
                              << "Multi-threaded LLVM is required for parallel algorithms." << std::endl;
                    assert_unreachable( "LLVM error" );
                }
            }
        };
    }

    struct LLVM : intern::LLVM, intern::LLVMInit {
        bool select( const Meta &meta ) const override {
            return intern::LLVM::select( meta ) && !meta.input.probabilistic;
        }
    };
    struct ProbabilisticLLVM : intern::ProbabilisticLLVM, intern::LLVMInit {
        bool select( const Meta &meta ) const override {
            return intern::ProbabilisticLLVM::select( meta ) && meta.input.probabilistic;
        }
    };

    namespace intern {
        GENERATOR( Explicit, dess::extension, "Explicit",
                Not< algorithm::GenExplicit >, "divine/generator/explicit.h", &Traits::dess );
        GENERATOR( ProbabilisticExplicit, dess::extension, "Probabilistic explicit",
                Not< algorithm::GenExplicit >, "divine/generator/explicit.h", &Traits::dess );
    }
    struct Explicit : public intern::Explicit {
        bool select( const Meta &meta ) const override {
            return intern::Explicit::select( meta ) &&
                dess::Header::fromFile( meta.input.model ).labelSize == 0;
        }
    };
    struct ProbabilisticExplicit : public intern::ProbabilisticExplicit {
        bool select( const Meta &meta ) const override {
            return intern::ProbabilisticExplicit::select( meta )
                && dess::Header::fromFile( meta.input.model )
                    .capabilities.has( dess::Capability::Probability );
        }
    };

    namespace intern {
        GENERATOR( Dummy, nullptr, "Dummy", Any, "divine/generator/dummy.h", !Tr( &Traits::small ) );
    }
    struct Dummy : public intern::Dummy {
        bool select( const Meta &meta ) const override {
            return meta.input.dummygen;
        }
    };

    using Generators = Select< Dve, Coin, LLVM, ProbabilisticLLVM, Timed, CESMI,
                 ProbabilisticExplicit, Explicit, Dummy >::WarnUnavailable;

#undef GENERATOR
}

namespace transform {
    struct IsTransformT { };

#define TRANSFORM( TRANS, SYMBOL, SELECTOR, SUPPORTED_BY, HEADER, TR ) struct TRANS : virtual Selectable { \
        using IsTransform = IsTransformT; \
        static constexpr const char *header = HEADER; \
        static constexpr const char *symbol = "template< typename Graph, " \
                "typename Store, typename Stat >\n" \
                "using _Transform = " SYMBOL ";\n"; \
        static constexpr const char *key = #TRANS; \
        using SupportedBy = SUPPORTED_BY; \
        bool select( const Meta &meta ) const override { \
            return SELECTOR; \
            static_cast< void >( meta ); \
        } \
        TRAIT( TR ) \
        ATOM( TRANS ) \
    }

    TRANSFORM( None, "::divine::graph::NonPORGraph< Graph, Store >", true,
            Any, "divine/graph/por.h", &Traits::always );

    using ForReductions = And< generator::Dve, Not< algorithm::Info > >;
    TRANSFORM( Fairness, "::divine::graph::FairGraph< Graph, Store >",
            meta.algorithm.fairness, ForReductions, "divine/graph/fairness.h",
            !Tr( &Traits::small ) );
    TRANSFORM( POR, "::divine::algorithm::PORGraph< Graph, Store, Stat >",
            meta.algorithm.reduce.count( graph::R_POR ), ForReductions,
            "divine/algorithm/por-c3.h", !Tr( &Traits::small ) );
#undef TRANSFORM

    using Transforms = Select< POR, Fairness, None >::WarnOtherAvailable
        ::LastAvailableIsDefault::WarnUnavailable;
}

namespace visitor {
    struct IsVisitorT { };

#define VISITOR( VISIT, SELECTOR, SUPPORTED_BY, Tr ) struct VISIT : virtual Selectable { \
        using IsVisitor = IsVisitorT; \
        static constexpr const char *symbol = \
                    "using _Visitor = ::divine::visitor::" #VISIT ";\n" \
                    "using _TableProvider = ::divine::visitor::" #VISIT "Provider;\n"; \
        static constexpr const char *key = #VISIT; \
        using SupportedBy = SUPPORTED_BY; \
        bool select( const Meta &meta ) const override { \
            return SELECTOR; \
            static_cast< void >( meta ); \
        } \
        TRAIT( Tr ) \
        ATOM( VISIT ) \
    }

    VISITOR( Partitioned, true, Any, &Traits::always );
    using ForShared = Not< Or< algorithm::Simulate, algorithm::GenExplicit, algorithm::Info > >;
    VISITOR( Shared, meta.algorithm.sharedVisitor, ForShared, &Traits::always );
#undef VISITOR

    using Visitors = Select< Shared, Partitioned >::LastAvailableIsDefault::WarnUnavailable;
}

namespace store {
    struct IsStoreT { };

#define STORE( STOR, SELECTOR, SUPPORTED_BY, Tr ) struct STOR : virtual Selectable { \
        using IsStore = IsStoreT; \
        static constexpr const char *symbol = \
            "template < typename Provider, typename Generator, typename Hasher, typename Stat >\n" \
            "using _Store = ::divine::visitor::" #STOR "< Provider, Generator, Hasher, Stat >;\n"; \
        static constexpr const char *key = #STOR; \
        using SupportedBy = SUPPORTED_BY; \
        bool select( const Meta &meta ) const override { \
            return SELECTOR; \
            static_cast< void >( meta ); \
        } \
        TRAIT( Tr ) \
        ATOM( STOR ) \
    }

using ForCompressions = Not< Or< algorithm::Info, algorithm::Simulate > >;
    STORE( NTreeStore, meta.algorithm.compression == meta::Algorithm::Compression::Tree,
            ForCompressions, &Traits::ntree );
    STORE( HcStore, meta.algorithm.hashCompaction, ForCompressions, &Traits::hc );
    STORE( DefaultStore, true, Any, &Traits::always );

    using Stores = Select< NTreeStore, HcStore, DefaultStore >
            ::WarnOtherAvailable::WarnUnavailable::LastAvailableIsDefault;
}

namespace topology {
    struct IsTopologyT { };

#define TOPOLOGY( TOPO, SELECTOR, SUPPORTED_BY, Tr ) struct TOPO : virtual Selectable { \
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
        bool select( const Meta &meta ) const override { \
            return SELECTOR; \
            static_cast< void >( meta ); \
        } \
        TRAIT( Tr ) \
        ATOM( TOPO ) \
    }

#ifndef O_PERFORMANCE
    using ForMpi = Any; // as we need NDFS without O_PERFORMANCE
#else
    using ForMpi = Not< Or< algorithm::NestedDFS, algorithm::Info, visitor::Shared > >;
#endif

    TOPOLOGY( Mpi, meta.execution.nodes > 1, ForMpi, !Tr( &Traits::performance ) || &Traits::mpi );
    TOPOLOGY( Local, true, Any, &Traits::performance );

#undef TOPOLOGY

    using Topologies = Select< Mpi, Local >::LastAvailableIsDefault;
}

namespace statistics {
    struct IsStatisticsT { };

#define STATISTICS( STAT, SELECTOR, SUPPORTED_BY, Tr ) struct STAT : virtual Selectable { \
        using IsStatistics = IsStatisticsT; \
        static constexpr const char *symbol = "using _Statistics = ::divine::" #STAT ";\n"; \
        static constexpr const char *key = #STAT; \
        static constexpr const char *header = "divine/utility/statistics.h"; \
        using SupportedBy = SUPPORTED_BY; \
        bool select( const Meta &meta ) const override { \
            return SELECTOR; \
            static_cast< void >( meta ); \
        } \
        TRAIT( Tr ) \
        ATOM( STAT ) \
    }

    STATISTICS( TrackStatistics, meta.output.statistics, Any, &Traits::always );
    using ForNoStat = Not< Or< algorithm::Info, algorithm::Simulate > >;
    STATISTICS( NoStatistics, true, ForNoStat, &Traits::performance );
#undef STATISTICS

    using Statistics = Select< TrackStatistics, NoStatistics >::LastAvailableIsDefault;
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

#undef TRAIT

#endif // DIVINE_INSTANCES_DEFINITIONS
