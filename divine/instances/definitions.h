// -*- C++ -*- (c) 2013 Vladimír Štill <xstill@fi.muni.cz>

#include <divine/utility/meta.h>
#include <divine/llvm/support.h>
#include <divine/generator/cesmi.h>
#include <divine/explicit/header.h>

#include <divine/toolkit/typelist.h>
#include <wibble/strongenumflags.h>
#include <wibble/fixarray.h>
#include <wibble/union.h>
#include <wibble/mixin.h>
#include <wibble/sfinae.h>
#include <array>
#include <map>

#ifndef DIVINE_INSTANCES_DEFINITIONS
#define DIVINE_INSTANCES_DEFINITIONS

namespace divine {
namespace instantiate {

enum class SelectOption { WarnOther = 0x1, LastDefault = 0x2, WarnUnavailable = 0x4, ErrUnavailable = 0x8 };
using SelectOptions = wibble::StrongEnumFlags< SelectOption >;
using wibble::operator|;

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

        bool operator()( const Traits &tr ) const { return fn( tr ); }

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

enum class Type {
    Begin,
    Algorithm, Generator, Transform, Visitor, Store, Topology, Statistics,
    End
};

// std::map does not allow operator [] on const
template< typename K, typename T >
struct CMap : std::map< K, T > {
    CMap( std::initializer_list< std::pair< const K, T > > init ) : std::map< K, T >( init ) { }

    const T &operator[]( const K &k ) const {
        auto it = this->find( k );
        assert( it != this->end() );
        return it->second;
    }
};

// options specifiing how selction should work
const CMap< Type, SelectOptions > options {
    { Type::Algorithm,  SelectOption::ErrUnavailable },
    { Type::Generator,  SelectOption::ErrUnavailable },
    { Type::Transform,  SelectOption::WarnOther | SelectOption::WarnUnavailable | SelectOption::LastDefault },
    { Type::Visitor,    SelectOption::ErrUnavailable },
    { Type::Store,      SelectOption::WarnUnavailable | SelectOption::LastDefault },
    { Type::Topology,   SelectOption::LastDefault },
    { Type::Statistics, SelectOption::LastDefault }
};

enum class Algorithm {
    Begin,
    NestedDFS, Owcty, Map, Reachability, Metrics, Simulate, GenExplicit, Draw, Info,
    End
};
enum class Generator {
    Begin,
    Dve, Coin, LLVM, ProbabilisticLLVM, Timed, CESMI, ProbabilisticExplicit, Explicit, Dummy,
    End
};
enum class Transform {
    Begin,
    POR, Fairness, None,
    End
};
enum class Visitor {
    Begin,
    Shared, Partitioned,
    End
};
enum class Store {
    Begin,
    NTreeStore, HcStore, DefaultStore,
    End
};
enum class Topology {
    Begin,
    Mpi, Local,
    End
};
enum class Statistics {
    Begin,
    TrackStatistics, NoStatistics,
    End
};

// discriminated union of components
struct Key : wibble::mixin::LexComparable< Key > {
    Type type;
    int key;

    Key() : type( Type( -1 ) ), key( -1 ) { }

    Key( Algorithm alg ) : type( Type::Algorithm ), key( int( alg ) ) { }
    Key( Generator gen ) : type( Type::Generator ), key( int( gen ) ) { }
    Key( Transform tra ) : type( Type::Transform ), key( int( tra ) ) { }
    Key( Visitor vis )   : type( Type::Visitor ),   key( int( vis ) ) { }
    Key( Store stor )    : type( Type::Store ),     key( int( stor ) ) { }
    Key( Topology top )  : type( Type::Topology ),  key( int( top ) ) { }
    Key( Statistics st ) : type( Type::Statistics ),key( int( st ) ) { }

    std::tuple< Type, int > toTuple() const { return std::make_tuple( type, key ); }
};

static inline std::tuple< std::string, std::string > showGen( Key component ) {
#define SHOW( TYPE, COMPONENT ) if ( component == TYPE::COMPONENT ) return std::make_tuple( #TYPE, #COMPONENT )
    SHOW( Algorithm, NestedDFS );
    SHOW( Algorithm, Owcty );
    SHOW( Algorithm, Map );
    SHOW( Algorithm, Reachability );
    SHOW( Algorithm, Metrics );
    SHOW( Algorithm, Simulate );
    SHOW( Algorithm, GenExplicit );
    SHOW( Algorithm, Draw );
    SHOW( Algorithm, Info );

    SHOW( Generator, Dve );
    SHOW( Generator, Coin );
    SHOW( Generator, LLVM );
    SHOW( Generator, ProbabilisticLLVM );
    SHOW( Generator, Timed );
    SHOW( Generator, CESMI );
    SHOW( Generator, ProbabilisticExplicit );
    SHOW( Generator, Explicit );
    SHOW( Generator, Dummy );

    SHOW( Transform, POR );
    SHOW( Transform, Fairness );
    SHOW( Transform, None );

    SHOW( Visitor, Shared );
    SHOW( Visitor, Partitioned );

    SHOW( Store, NTreeStore );
    SHOW( Store, HcStore );
    SHOW( Store, DefaultStore );

    SHOW( Topology, Mpi );
    SHOW( Topology, Local );

    SHOW( Statistics, TrackStatistics );
    SHOW( Statistics, NoStatistics );

    std::string emsg = "show: unhandled option ( " + std::to_string( int( component.type ) ) + ", " + std::to_string( component.key ) + " )";
    assert_unreachable( emsg.c_str() );
#undef SHOW
}

static inline std::string show( Key component ) {
    std::string a, b;
    std::tie( a, b ) = showGen( component );
    return a + "::" + b;
}

using Instantiation = TypeList< Algorithm, Generator, Transform, Visitor, Store, Topology, Statistics >;
using InstT = std::array< std::vector< Key >, Instantiation::length >;

template< size_t i, typename I >
static inline InstT _buildInst( InstT &&inst, I ) {
    using Head = typename I::Head;
    for ( int j = int( Head::Begin ) + 1; j < int( Head::End ); ++j ) {
        std::get< i >( inst ).emplace_back( Head( j ) );
    }
    return _buildInst< i + 1 >( std::move( inst ), typename I::Tail() );
}

template<>
inline InstT _buildInst< Instantiation::length, TypeList<> >( InstT &&inst, TypeList<> ) {
    return inst;
}

const InstT instantiation = _buildInst< 0 >( InstT(), Instantiation() );

static bool constTrue( const Meta & ) { return true; }

// list of headers for each component -- optional
static const CMap< Key, std::vector< std::string > > headers = {
    { Algorithm::NestedDFS,    { "divine/algorithm/nested-dfs.h" } },
    { Algorithm::Owcty,        { "divine/algorithm/owcty.h" } },
    { Algorithm::Map,          { "divine/algorithm/map.h" } },
    { Algorithm::Reachability, { "divine/algorithm/reachability.h" } },
    { Algorithm::Metrics,      { "divine/algorithm/metrics.h" } },
    { Algorithm::Simulate,     { "divine/algorithm/simulate.h" } },
    { Algorithm::Draw,         { "tools/draw.h" } },
    { Algorithm::Info,         { "tools/info.h" } },
    { Algorithm:: GenExplicit, { "divine/algorithm/genexplicit.h" } },

    { Generator::Dve,                   { "divine/generator/dve.h" } },
    { Generator::Coin,                  { "divine/generator/coin.h"} },
    { Generator::Timed,                 { "divine/generator/timed.h" } },
    { Generator::CESMI,                 { "divine/generator/cesmi.h" } },
    { Generator::LLVM,                  { "divine/generator/llvm.h" } },
    { Generator::ProbabilisticLLVM,     { "divine/generator/llvm.h" } },
    { Generator::Explicit,              { "divine/generator/explicit.h" } },
    { Generator::ProbabilisticExplicit, { "divine/generator/explicit.h" } },
    { Generator::Dummy,                 { "divine/generator/dummy.h" } },

    { Transform::None,     { "divine/graph/por.h" } },
    { Transform::Fairness, { "divine/graph/fairness.h" } },
    { Transform::POR,      { "divine/algorithm/por-c3.h" } },

    { Statistics::NoStatistics,    { "divine/utility/statistics.h" } },
    { Statistics::TrackStatistics, { "divine/utility/statistics.h" } }
};

struct AlgSelect {
    meta::Algorithm::Type alg;
    AlgSelect( meta::Algorithm::Type alg ) : alg( alg ) { }

    bool operator()( const Meta &meta ) {
        return meta.algorithm.algorithm == alg;
    }
};

struct GenSelect {
    std::string extension;
    bool useProb;
    bool prob;
    GenSelect( std::string extension ) : extension( extension ), useProb( false ) { }
    GenSelect( std::string extension, bool probabilistic ) :
        extension( extension ), useProb( true ), prob( probabilistic )
    { }

    bool operator()( const Meta &meta ) {
        return wibble::str::endsWith( meta.input.model, extension )
            && (!useProb || (prob == meta.input.probabilistic));
    }
};

// selection function -- mandatory
static const CMap< Key, std::function< bool( const Meta & ) > > select = {
    { Algorithm::NestedDFS,    AlgSelect( meta::Algorithm::Type::Ndfs ) },
    { Algorithm::Owcty,        AlgSelect( meta::Algorithm::Type::Owcty ) },
    { Algorithm::Map,          AlgSelect( meta::Algorithm::Type::Map ) },
    { Algorithm::Reachability, AlgSelect( meta::Algorithm::Type::Reachability ) },
    { Algorithm::Metrics,      AlgSelect( meta::Algorithm::Type::Metrics ) },
    { Algorithm::Simulate,     AlgSelect( meta::Algorithm::Type::Simulate ) },
    { Algorithm::Draw,         AlgSelect( meta::Algorithm::Type::Draw ) },
    { Algorithm::Info,         AlgSelect( meta::Algorithm::Type::Info ) },
    { Algorithm::GenExplicit,  AlgSelect( meta::Algorithm::Type::GenExplicit ) },

    { Generator::Dve,               GenSelect( ".dve" ) },
    { Generator::Coin,              GenSelect( ".coin" ) },
    { Generator::Timed,             GenSelect( ".xml" ) },
    { Generator::CESMI,             GenSelect( generator::cesmi_ext ) },
    { Generator::LLVM,              GenSelect( ".bc", false ) },
    { Generator::ProbabilisticLLVM, GenSelect( ".bc", true ) },
    { Generator::Explicit,
        []( const Meta &meta ) {
            return GenSelect( dess::extension )( meta ) &&
                dess::Header::fromFile( meta.input.model ).labelSize == 0;
        } },
    { Generator::ProbabilisticExplicit,
        []( const Meta &meta ) {
            return GenSelect( dess::extension )( meta ) &&
                dess::Header::fromFile( meta.input.model )
                    .capabilities.has( dess::Capability::Probability );
        } },
    { Generator::Dummy, []( const Meta &meta ) { return meta.input.dummygen; } },

    { Transform::None,     constTrue },
    { Transform::Fairness, []( const Meta &meta ) { return meta.algorithm.fairness; } },
    { Transform::POR,      []( const Meta &meta ) { return meta.algorithm.reduce.count( graph::R_POR ); } },

    { Visitor::Partitioned, constTrue },
    { Visitor::Shared, []( const Meta &meta ) { return meta.algorithm.sharedVisitor; } },

    { Store::DefaultStore, constTrue },
    { Store::NTreeStore,   []( const Meta &meta ) { return meta.algorithm.compression == meta::Algorithm::Compression::Tree; } },
    { Store::HcStore,      []( const Meta &meta ) { return meta.algorithm.hashCompaction; } },

    { Topology::Mpi,   []( const Meta &meta ) { return meta.execution.nodes > 1; } },
    { Topology::Local, constTrue },

    { Statistics::TrackStatistics, []( const Meta &meta ) { return meta.output.statistics; } },
    { Statistics::NoStatistics,    constTrue }
};

struct NameAlgo {
    std::string name;
    NameAlgo( std::string name ) : name( name ) { }
    void operator()( Meta &meta ) { meta.algorithm.name = name; }
};

struct NameGen {
    std::string name;
    NameGen( std::string name ) : name( name ) { }
    void operator()( Meta &meta ) { meta.input.modelType = name; }
};

// post select meta modification -- optional
static const CMap< Key, std::function< void( Meta & ) > > postSelect = {
    { Algorithm::NestedDFS,    NameAlgo( "Nested DFS" ) },
    { Algorithm::Owcty,        NameAlgo( "OWCTY" ) },
    { Algorithm::Map,          NameAlgo( "MAP" ) },
    { Algorithm::Reachability, NameAlgo( "Reachability" ) },
    { Algorithm::Metrics,      NameAlgo( "Metrics" ) },
    { Algorithm::Simulate,     NameAlgo( "Simulate" ) },
    { Algorithm::Draw,         NameAlgo( "Draw" ) },
    { Algorithm::Info,         NameAlgo( "Info" ) },
    { Algorithm::GenExplicit,  NameAlgo( "Gen-Explicit" ) },

    { Generator::Dve,                   NameGen( "DVE" ) },
    { Generator::Coin,                  NameGen( "CoIn" ) },
    { Generator::Timed,                 NameGen( "Timed" ) },
    { Generator::CESMI,                 NameGen( "CESMI" ) },
    { Generator::LLVM,                  NameGen( "LLVM" ) },
    { Generator::ProbabilisticLLVM,     NameGen( "LLVM (probabilistic)" ) },
    { Generator::Explicit,              NameGen( "Explicit" ) },
    { Generator::ProbabilisticExplicit, NameGen( "Explicit (probabilistic)" ) },
    { Generator::Dummy,                 NameGen( "Dummy" ) },

    { Transform::POR,      []( Meta &meta ) { meta.algorithm.fairness = false; meta.algorithm.reduce.insert( graph::R_POR ); } },
    { Transform::Fairness, []( Meta &meta ) { meta.algorithm.fairness = true; meta.algorithm.reduce.erase( graph::R_POR ); } },
    { Transform::None,     []( Meta &meta ) { meta.algorithm.fairness = false; meta.algorithm.reduce.erase( graph::R_POR ); } }
};

static void initLLVM( const Meta &meta ) {
    if ( meta.execution.threads > 1 && !llvm::initMultithreaded() ) {
        std::cerr << "FATAL: This binary is linked to single-threaded LLVM." << std::endl
                  << "Multi-threaded LLVM is required for parallel algorithms." << std::endl;
        assert_unreachable( "LLVM error" );
    }
}

// aditional initialization of runtime parameters -- optional
static const CMap< Key, std::function< void( const Meta & ) > > init = {
    { Generator::LLVM,              initLLVM },
    { Generator::ProbabilisticLLVM, initLLVM },
};

// traits can be used to specify required configure time options for component -- optional
static const CMap< Key, Traits::Get > traits = {
    { Algorithm::GenExplicit, &Traits::dess },

    { Generator::Dve,                   &Traits::dve },
    { Generator::Coin,                  &Traits::coin },
    { Generator::Timed,                 &Traits::timed },
    { Generator::CESMI,                 &Traits::cesmi },
    { Generator::LLVM,                  &Traits::llvm },
    { Generator::ProbabilisticLLVM,     &Traits::llvm },
    { Generator::Explicit,              &Traits::dess },
    { Generator::ProbabilisticExplicit, &Traits::dess },
    { Generator::Dummy,                 !Tr( &Traits::small ) },

    { Transform::POR, !Tr( &Traits::small ) },
    { Transform::Fairness, !Tr( &Traits::small ) },

    { Store::NTreeStore, &Traits::ntree },
    { Store::HcStore,    &Traits::hc },

    { Topology::Mpi,   !Tr( &Traits::performance ) || &Traits::mpi },
    { Topology::Local, &Traits::performance },

    { Statistics::NoStatistics, &Traits::performance }
};

// algoritm symbols -- mandatory for each algorithm
static const CMap< Algorithm, std::string > algorithm = {
    { Algorithm::NestedDFS,    "divine::algorithm::NestedDFS" },
    { Algorithm::Owcty,        "divine::algorithm::Owcty" },
    { Algorithm::Map,          "divine::algorithm::Map" },
    { Algorithm::Reachability, "divine::algorithm::Reachability" },
    { Algorithm::Metrics,      "divine::algorithm::Metrics" },
    { Algorithm::Simulate,     "divine::algorithm::Simulate" },
    { Algorithm::Draw,         "divine::Draw" },
    { Algorithm::Info,         "divine::Info" },
    { Algorithm::GenExplicit,  "divine::algorithm::GenExplicit" },
};

struct SupportedBy;

struct And {
    And( std::initializer_list< SupportedBy > init ) : val( init ) { }
    wibble::FixArray< SupportedBy > val;
};
struct Or {
    Or( std::initializer_list< SupportedBy > init ) : val( init ) { }
    wibble::FixArray< SupportedBy > val;
};

template< typename SuppBy >
static std::unique_ptr< SupportedBy > box( const SuppBy & );

struct Not {
    template< typename SuppBy >
    explicit Not( SuppBy sb ) : val( box( sb ) ) { }
    Not( const Not &other ) : val( box( *other.val.get() ) ) { }

    std::unique_ptr< SupportedBy > val;
};

struct SupportedBy : wibble::Union< And, Or, Not, Key > {
    template< typename... Args >
    SupportedBy( Args &&...args ) :
        wibble::Union< And, Or, Not, Key >( std::forward< Args >( args )... )
    { }
};

template< typename SuppBy >
static inline std::unique_ptr< SupportedBy > box( const SuppBy &sb ) {
    return std::unique_ptr< SupportedBy >( new SupportedBy( sb ) );
}

// Specification of supported component combination,
// the component later in instatiation can depend only on components
// instantiated before it
static const CMap< Key, SupportedBy > supportedBy = {
    { Generator::Explicit,              Not{ Algorithm::GenExplicit } },
    { Generator::ProbabilisticExplicit, Not{ Algorithm::GenExplicit } },

    { Transform::POR,      And{ Generator::Dve, Not{ Algorithm::Info } } },
    { Transform::Fairness, And{ Generator::Dve, Not{ Algorithm::Info } } },

    { Visitor::Shared, Not{ Or{ Algorithm::Simulate, Algorithm::GenExplicit, Algorithm::Info } } },

    { Store::NTreeStore, Not{ Or{ Algorithm::Info, Algorithm::Simulate } } },
    { Store::HcStore,    Not{ Or{ Algorithm::Info, Algorithm::Simulate } } },

#ifdef O_PERFORMANCE
    { Topology::Mpi, Not{ Or{ Algorithm::NestedDFS, Algorithm::Info, Visitor::Shared } } },
#endif

    { Statistics::NoStatistics, Not{ Or{ Algorithm::Info, Algorithm::Simulate } } },
};

using SymbolPair = std::pair< const Key, wibble::FixArray< std::string > >;
static inline SymbolPair symGen( Generator g ) {
    return { g, { "using _Generator = ::divine::generator::" + std::get< 1 >( showGen( g ) ) + ";" } };
}

static inline SymbolPair symTrans( Transform t, std::string symbol ) {
    return { t, { "template< typename Graph, typename Store, typename Stat >",
                  "using _Transform = " + symbol + ";" } };
}

static inline SymbolPair symVis( Visitor v ) {
    auto vis = std::get< 1 >( showGen( v ) );
    return { v, { "using _Visitor = ::divine::visitor::" + vis + ";",
                  "using _TableProvider = ::divine::visitor::" + vis + "Provider;" } };
}

static inline SymbolPair symStore( Store s ) {
    auto stor = std::get< 1 >( showGen( s ) );
    return { s, { "template < typename Provider, typename Generator, typename Hasher, typename Stat >",
                  "using _Store = ::divine::visitor::" + stor + "< Provider, Generator, Hasher, Stat >;" } };
}

static inline SymbolPair symTopo( Topology t ) {
    auto topo = std::get< 1 >( showGen( t ) );
    return { t, { "template< typename Transition >",
                  "struct _Topology {",
                  "    template< typename I >",
                  "    using T = typename ::divine::Topology< Transition >",
                  "                  ::template " + topo + "< I >;",
                  "};" } };
}

static inline SymbolPair symStat( Statistics s ) {
    auto stat = std::get< 1 >( showGen( s ) );
    return { s, { "using _Statistics = ::divine::" + stat + ";" } };
}

// symbols for components with exception of algorithm -- mandatory (except for algorithm)
static const CMap< Key, wibble::FixArray< std::string > > symbols = {
    symGen( Generator::Dve ),
    symGen( Generator::Coin ),
    symGen( Generator::Timed ),
    symGen( Generator::CESMI ),
    symGen( Generator::LLVM ),
    symGen( Generator::ProbabilisticLLVM ),
    symGen( Generator::Explicit ),
    symGen( Generator::ProbabilisticExplicit ),
    symGen( Generator::Dummy ),

    symTrans( Transform::None,     "::divine::graph::NonPORGraph< Graph, Store >" ),
    symTrans( Transform::Fairness, "::divine::graph::FairGraph< Graph, Store >" ),
    symTrans( Transform::POR,      "::divine::algorithm::PORGraph< Graph, Store, Stat >" ),

    symVis( Visitor::Partitioned ),
    symVis( Visitor::Shared ),

    symStore( Store::NTreeStore ),
    symStore( Store::HcStore ),
    symStore( Store::DefaultStore ),

    symTopo( Topology::Mpi ),
    symTopo( Topology::Local ),

    symStat( Statistics::NoStatistics ),
    symStat( Statistics::TrackStatistics )
};

template< typename Graph, typename Store >
using Transition = std::tuple< typename Store::Vertex, typename Graph::Node, typename Graph::Label >;

static inline bool _select( const Meta &meta, Key k ) {
    return select[ k ]( meta );
}

static inline bool _traits( Key k ) {
    auto it = traits.find( k );
    return it == traits.end() || it->second( Traits::available() );
}

static inline void _postSelect( Meta &meta, Key k ) {
    auto it = postSelect.find( k );
    if ( it != postSelect.end() )
        it->second( meta );
}

static inline void _init( const Meta &meta, wibble::FixArray< Key > instance ) {
    for ( auto k : instance ) {
        auto it = init.find( k );
        if ( it != init.end() )
            it->second( meta );
    }
}

static inline void _warningMissing( Key key ) {
    std::cerr << "WARNING: The " + std::get< 1 >( showGen( key ) ) + " is not available "
              << "while selecting " + std::get< 0 >( showGen( key ) ) + "." << std::endl
              << "    Will try to select other option." << std::endl;
}

static inline std::nullptr_t _errorMissing( wibble::FixArray< Key > trace ) {
    std::vector< std::string > strace;
    for ( auto x : trace )
        strace.push_back( show( x ) );
    std::cerr << "ERROR: The " << std::get< 1 >( showGen( trace.back() ) ) << " is not available "
              << "while selecting " << std::get< 0 >( showGen( trace.back() ) ) << "." << std::endl
              << "    Selector come to dead end after selecting: "
              << wibble::str::fmt_container( strace, '[', ']' ) << std::endl;
    return nullptr;
}

static inline std::nullptr_t _noDefault( wibble::FixArray< Key > trace ) {
    std::vector< std::string > strace;
    for ( auto x : trace )
        strace.push_back( show( x ) );
    std::cerr << "ERROR: Selector come to dead end after selecting: "
              << wibble::str::fmt_container( strace, '[', ']' ) << std::endl
              << "    No default available." << std::endl;
    return nullptr;
}

static inline void _warnOtherAvailable( Meta metacopy, Key k ) {
    for ( auto i : instantiation ) {
        if ( i[ 0 ].type == k.type ) {
            std::vector< std::string > skipped;
            for ( auto l : i ) {
                if ( l.key > k.key && _select( metacopy, l ) )
                    skipped.push_back( std::get< 1 >( showGen( l ) ) );
            }
            if ( options[ k.type ].has( SelectOption::LastDefault ) && skipped.size() )
                skipped.pop_back();

            if ( skipped.empty() )
                return;

            std::cerr << "WARNING: symbols " << wibble::str::fmt_container( skipped, '{', '}' )
                      << " were not selected because " << std::get< 1 >( showGen( k ) )
                      << " has higher priority." << std::endl
                      << "    When instantiating " << std::get< 0 >( showGen( k ) ) << "." << std::endl;
            return;
        }
    }
    assert_unreachable( "Invalid key" );
}

}
}

#endif // DIVINE_INSTANCES_DEFINITIONS
