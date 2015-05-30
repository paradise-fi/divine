// -*- C++ -*- (c) 2013 Vladimír Štill <xstill@fi.muni.cz>
//                 2013 Petr Ročkai <me@mornfall.net>

#include <array>
#include <map>

#include <brick-types.h>
#include <brick-hlist.h>
#include <brick-string.h>

#include <divine/utility/meta.h>
#include <divine/llvm/support.h>
#include <divine/generator/cesmi.h>
#include <divine/explicit/header.h>
#include <divine/instances/select.h>

#ifndef DIVINE_INSTANCES_DEFINITIONS
#define DIVINE_INSTANCES_DEFINITIONS

namespace divine {
namespace instantiate {

template< typename T >
using FixArray = std::vector< T >;

template< typename T >
FixArray< T > appendArray( FixArray< T > arr, T val ) {
    arr.resize( arr.size() + 1 );
    arr.back() = std::move( val );
    return arr;
}

enum class SelectOption { WarnOther = 0x1, LastDefault = 0x2, WarnUnavailable = 0x4, ErrUnavailable = 0x8 };
using SelectOptions = brick::types::StrongEnumFlags< SelectOption >;
using brick::types::operator|;

struct Traits {
    /* this is kind of boilerplate code for accessing compile time macro definitions
     * at runtime
     *
     * no bitfiels -- pointers to members will be taken
     */
    bool dev_nopools, dev_conflate;
    bool gen_dve, gen_llvm, gen_llvm_csdr, gen_llvm_ptst, gen_llvm_prob,
         gen_timed, gen_coin, gen_cesmi, gen_explicit, gen_explicit_prob,
         gen_dummy;
    bool alg_owcty, alg_map, alg_metrics, alg_ndfs, alg_reachability,
         alg_weakreachability, alg_explicit, alg_csdr;
    bool store_hc, store_compress;
    bool transform_por, transform_fair;
    bool opt_draw, opt_simulate, opt_mpi, opt_jit;

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
        t.dev_conflate = DEV_CONFLATE;
        t.dev_nopools = DEV_NOPOOLS;

        t.gen_llvm = GEN_LLVM;
        t.gen_llvm_csdr = GEN_LLVM_CSDR;
        t.gen_llvm_prob = GEN_LLVM_PROB;
        t.gen_llvm_ptst = GEN_LLVM_PTST;
        t.gen_dve = GEN_DVE;
        t.gen_coin = GEN_COIN;
        t.gen_cesmi = GEN_CESMI;
        t.gen_timed = GEN_TIMED;
        t.gen_explicit = GEN_EXPLICIT;
        t.gen_explicit_prob = GEN_EXPLICIT_PROB;
        t.gen_dummy = GEN_DUMMY;

        t.alg_map = ALG_MAP;
        t.alg_owcty = ALG_OWCTY;
        t.alg_reachability = ALG_REACHABILITY;
        t.alg_weakreachability = ALG_WEAKREACHABILITY;
        t.alg_explicit = ALG_EXPLICIT;
        t.alg_csdr = ALG_CSDR;
        t.alg_ndfs = ALG_NDFS;
        t.alg_metrics = ALG_METRICS;

        t.store_hc = STORE_HC;
        t.store_compress = STORE_COMPRESS;
        t.transform_por = TRANSFORM_POR;
        t.transform_fair = TRANSFORM_FAIR;

        t.opt_mpi = OPT_MPI;
        t.opt_simulate = OPT_SIMULATE;
        t.opt_draw = OPT_DRAW;
        t.opt_jit = false; // OPT_JIT;

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
        ASSERT( it != this->end() );
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
    NestedDFS, Owcty, Map, Reachability, WeakReachability, Csdr, Metrics, Simulate, GenExplicit, Draw, Info,
    End
};
enum class Generator {
    Begin,
    LLVM, PointsToLLVM, ControlLLVM, ProbabilisticLLVM,
    Dve, Coin, Timed, CESMI, ProbabilisticExplicit, Explicit, Dummy,
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
    NDFSNTreeStore, NTreeStore, HcStore, DefaultStore,
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
struct Key : brick::types::mixin::LexComparable< Key > {
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

using Trace = std::vector< instantiate::Key >;
extern const CMap< Trace, AlgorithmPtr (*)( Meta & ) > jumptable;

static inline std::tuple< std::string, std::string > showGen( Key component ) {
#define SHOW( TYPE, COMPONENT ) if ( component == TYPE::COMPONENT ) return std::make_tuple( #TYPE, #COMPONENT )
    SHOW( Algorithm, NestedDFS );
    SHOW( Algorithm, Owcty );
    SHOW( Algorithm, Map );
    SHOW( Algorithm, Reachability );
    SHOW( Algorithm, WeakReachability );
    SHOW( Algorithm, Csdr );
    SHOW( Algorithm, Metrics );
    SHOW( Algorithm, Simulate );
    SHOW( Algorithm, GenExplicit );
    SHOW( Algorithm, Draw );
    SHOW( Algorithm, Info );

    SHOW( Generator, Dve );
    SHOW( Generator, Coin );
    SHOW( Generator, LLVM );
    SHOW( Generator, PointsToLLVM );
    SHOW( Generator, ControlLLVM );
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

    SHOW( Store, NDFSNTreeStore );
    SHOW( Store, NTreeStore );
    SHOW( Store, HcStore );
    SHOW( Store, DefaultStore );

    SHOW( Topology, Mpi );
    SHOW( Topology, Local );

    SHOW( Statistics, TrackStatistics );
    SHOW( Statistics, NoStatistics );

    std::string emsg = "show: unhandled option ( " + std::to_string( int( component.type ) ) + ", " + std::to_string( component.key ) + " )";
    ASSERT_UNREACHABLE( emsg.c_str() );
#undef SHOW
}

static inline std::string show( Key component ) {
    std::string a, b;
    std::tie( a, b ) = showGen( component );
    return a + "::" + b;
}

static inline std::ostream &operator<<( std::ostream &o, Key k ) { return o << show( k ); }

using brick::hlist::TypeList;
using brick::types::Union;

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
    { Algorithm::WeakReachability, { "divine/algorithm/reachability.h" } },
    { Algorithm::Csdr,         { "divine/algorithm/csdr.h" } },
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
    { Generator::PointsToLLVM,          { "divine/generator/llvm.h" } },
    { Generator::ControlLLVM,           { "divine/generator/llvm.h" } },
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
    std::string property;

    GenSelect( std::string extension ) : extension( extension ), useProb( false ) { }
    GenSelect( std::string extension, bool probabilistic, std::string prop = "" ) :
        extension( extension ), useProb( true ), prob( probabilistic ), property( prop )
    { }

    bool operator()( const Meta &meta ) {
        return brick::string::endsWith( meta.input.model, extension )
            && (!useProb || (prob == meta.input.probabilistic))
            && (property.empty() || meta.input.properties.count( property ) );

    }
};

// selection function -- mandatory
static const CMap< Key, std::function< bool( const Meta & ) > > select = {
    { Algorithm::NestedDFS,    AlgSelect( meta::Algorithm::Type::Ndfs ) },
    { Algorithm::Owcty,        AlgSelect( meta::Algorithm::Type::Owcty ) },
    { Algorithm::Map,          AlgSelect( meta::Algorithm::Type::Map ) },
    { Algorithm::Reachability, AlgSelect( meta::Algorithm::Type::Reachability ) },
    { Algorithm::WeakReachability, AlgSelect( meta::Algorithm::Type::WeakReachability ) },
    { Algorithm::Csdr,         AlgSelect( meta::Algorithm::Type::Csdr ) },
    { Algorithm::Metrics,      AlgSelect( meta::Algorithm::Type::Metrics ) },
    { Algorithm::Simulate,     AlgSelect( meta::Algorithm::Type::Simulate ) },
    { Algorithm::Draw,         AlgSelect( meta::Algorithm::Type::Draw ) },
    { Algorithm::Info,         AlgSelect( meta::Algorithm::Type::Info ) },
    { Algorithm::GenExplicit,  AlgSelect( meta::Algorithm::Type::GenExplicit ) },

    { Generator::Dve,               GenSelect( ".dve" ) },
    { Generator::Coin,              GenSelect( ".coin" ) },
    { Generator::Timed,             GenSelect( ".xml" ) },
    { Generator::CESMI,             GenSelect( generator::cesmi_ext ) },
    { Generator::PointsToLLVM,      GenSelect( ".bc", false, "pointsto" ) },
    { Generator::LLVM,              GenSelect( ".bc", false ) }, // these two have
    { Generator::ControlLLVM,       GenSelect( ".bc", false ) }, // excluding supported-by
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

    { Store::NDFSNTreeStore,
        []( const Meta &meta ) {
            return AlgSelect( meta::Algorithm::Type::Ndfs )( meta ) &&
                meta.algorithm.compression == meta::Algorithm::Compression::Tree;
        } },
    { Store::DefaultStore,   constTrue },
    { Store::NTreeStore,     []( const Meta &meta ) { return meta.algorithm.compression == meta::Algorithm::Compression::Tree; } },
    { Store::HcStore,        []( const Meta &meta ) { return meta.algorithm.hashCompaction; } },

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
    { Algorithm::WeakReachability, NameAlgo( "Weak Reachability" ) },
    { Algorithm::Csdr,         NameAlgo( "CSDR" ) },
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
    { Generator::PointsToLLVM,          NameGen( "LLVM (pointsto)" ) },
    { Generator::ControlLLVM,           NameGen( "LLVM (control)" ) },
    { Generator::ProbabilisticLLVM,     NameGen( "LLVM (probabilistic)" ) },
    { Generator::Explicit,              NameGen( "Explicit" ) },
    { Generator::ProbabilisticExplicit, NameGen( "Explicit (probabilistic)" ) },
    { Generator::Dummy,                 NameGen( "Dummy" ) }
};

// those functions will be called if given component should be selected
// but it is not available
static const CMap< Key, std::function< void( Meta & ) > > deactivation = {
    { Transform::POR,      []( Meta &meta ) { meta.algorithm.reduce.erase( graph::R_POR ); } },
    { Transform::Fairness, []( Meta &meta ) { meta.algorithm.fairness = false; } },
};

static void initLLVM( const Meta &meta ) {
    if ( meta.execution.threads > 1 && !llvm::initMultithreaded() ) {
        std::cerr << "FATAL: This binary is linked to single-threaded LLVM." << std::endl
                  << "Multi-threaded LLVM is required for parallel algorithms." << std::endl;
        ASSERT_UNREACHABLE( "LLVM error" );
    }
}

// aditional initialization of runtime parameters -- optional
static const CMap< Key, std::function< void( const Meta & ) > > init = {
    { Generator::LLVM,              initLLVM },
    { Generator::PointsToLLVM,      initLLVM },
    { Generator::ControlLLVM,       initLLVM },
    { Generator::ProbabilisticLLVM, initLLVM },
};

// traits can be used to specify required configure time options for component -- optional
static const CMap< Key, Traits::Get > traits = {
    { Algorithm::GenExplicit,   &Traits::alg_explicit },
    { Algorithm::Owcty,            &Traits::alg_owcty },
    { Algorithm::Map,              &Traits::alg_map },
    { Algorithm::NestedDFS,        &Traits::alg_ndfs },
    { Algorithm::Csdr,             &Traits::alg_csdr },
    { Algorithm::Reachability,     &Traits::alg_reachability },
    { Algorithm::WeakReachability, &Traits::alg_weakreachability },
    { Algorithm::Metrics,          &Traits::alg_metrics },

    { Generator::Dve,                   &Traits::gen_dve },
    { Generator::Coin,                  &Traits::gen_coin },
    { Generator::Timed,                 &Traits::gen_timed },
    { Generator::CESMI,                 &Traits::gen_cesmi },
    { Generator::LLVM,                  &Traits::gen_llvm },
    { Generator::PointsToLLVM,          &Traits::gen_llvm_ptst },
    { Generator::ControlLLVM,           &Traits::gen_llvm_csdr },
    { Generator::ProbabilisticLLVM,     &Traits::gen_llvm_prob  },
    { Generator::Explicit,              &Traits::gen_explicit },
    { Generator::ProbabilisticExplicit, &Traits::gen_explicit_prob },
    { Generator::Dummy,                 &Traits::gen_dummy },

    { Transform::POR,      &Traits::transform_por },
    { Transform::Fairness, &Traits::transform_fair },

    { Store::NTreeStore, &Traits::store_compress },
    { Store::HcStore,    &Traits::store_hc },

    { Topology::Mpi,   Tr( &Traits::dev_conflate ) || Tr( &Traits::opt_mpi ) },
    { Topology::Local, !Tr( &Traits::dev_conflate ) },

    { Statistics::NoStatistics, !Tr( &Traits::dev_conflate ) }
};

// algoritm symbols -- mandatory for each algorithm
static const CMap< Algorithm, std::string > algorithm = {
    { Algorithm::NestedDFS,    "divine::algorithm::NestedDFS" },
    { Algorithm::Owcty,        "divine::algorithm::Owcty" },
    { Algorithm::Map,          "divine::algorithm::Map" },
    { Algorithm::Reachability, "divine::algorithm::Reachability" },
    { Algorithm::WeakReachability, "divine::algorithm::WeakReachability" },
    { Algorithm::Csdr,         "divine::algorithm::Csdr" },
    { Algorithm::Metrics,      "divine::algorithm::Metrics" },
    { Algorithm::Simulate,     "divine::algorithm::Simulate" },
    { Algorithm::Draw,         "divine::Draw" },
    { Algorithm::Info,         "divine::Info" },
    { Algorithm::GenExplicit,  "divine::algorithm::GenExplicit" },
};

struct SupportedBy;

struct And {
    And( std::initializer_list< SupportedBy > init ) : val( init ) { }
    FixArray< SupportedBy > val;
};
struct Or {
    Or( std::initializer_list< SupportedBy > init ) : val( init ) { }
    FixArray< SupportedBy > val;
};

template< typename SuppBy >
static std::unique_ptr< SupportedBy > box( const SuppBy & );

struct Not {
    template< typename SuppBy >
    explicit Not( SuppBy sb ) : val( box( sb ) ) { }
    Not( const Not &other ) : val( box( *other.val.get() ) ) { }

    std::unique_ptr< SupportedBy > val;
};

struct SupportedBy : Union< And, Or, Not, Key > {
    template< typename... Args >
    SupportedBy( Args &&...args ) :
        Union< And, Or, Not, Key >( std::forward< Args >( args )... )
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
    { Generator::Explicit,              Not{ Or{ Algorithm::GenExplicit, Algorithm::Csdr } } },
    { Generator::ProbabilisticExplicit, Not{ Or{ Algorithm::GenExplicit, Algorithm::Csdr } } },

    { Generator::Dummy,                 Not{ Algorithm::Csdr } },
    { Generator::Dve,                   Not{ Algorithm::Csdr } },
    { Generator::Coin,                  Not{ Algorithm::Csdr } },
    { Generator::CESMI,                 Not{ Algorithm::Csdr } },
    { Generator::Timed,                 Not{ Algorithm::Csdr } },
    { Generator::LLVM,                  Not{ Or{ Algorithm::Simulate, Algorithm::Csdr } } },
    { Generator::ProbabilisticLLVM,     Not{ Algorithm::Csdr } },
    { Generator::PointsToLLVM,          Not{ Algorithm::Csdr } },
    { Generator::ControlLLVM,
        Not{ Or{ Algorithm::Info, Algorithm::Draw, Algorithm::Metrics,
            Algorithm::Reachability, Algorithm::WeakReachability,
            Algorithm::NestedDFS, Algorithm::Map, Algorithm::Owcty,
            Algorithm::GenExplicit } } },

    { Transform::POR,      And{ Generator::Dve, Not{ Algorithm::Info } } },
    { Transform::Fairness, And{ Or{ Generator::Dve, Generator::LLVM, Generator::ControlLLVM, Generator::ProbabilisticLLVM },
                                Not{ Algorithm::Info } } },

    { Visitor::Shared, Not{ Or{ Algorithm::Simulate, Algorithm::Info } } },

    { Store::NDFSNTreeStore, Algorithm::NestedDFS },
    { Store::NTreeStore,     Not{ Or{ Algorithm::Info, Algorithm::NestedDFS } } },
    { Store::HcStore,        Not{ Or{ Algorithm::Info, Algorithm::Simulate } } },

#if !DEV_CONFLATE
    { Topology::Mpi, Not{ Or{ Algorithm::NestedDFS, Algorithm::Info, Visitor::Shared } } },
#endif

    { Statistics::NoStatistics, Not{ Or{ Algorithm::Info, Algorithm::Simulate } } },
};

using SymbolPair = std::pair< const Key, FixArray< std::string > >;
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
static const CMap< Key, FixArray< std::string > > symbols = {
    symGen( Generator::Dve ),
    symGen( Generator::Coin ),
    symGen( Generator::Timed ),
    symGen( Generator::CESMI ),
    symGen( Generator::LLVM ),
    symGen( Generator::PointsToLLVM ),
    symGen( Generator::ControlLLVM ),
    symGen( Generator::ProbabilisticLLVM ),
    symGen( Generator::Explicit ),
    symGen( Generator::ProbabilisticExplicit ),
    symGen( Generator::Dummy ),

    symTrans( Transform::None,     "::divine::graph::NonPORGraph< Graph, Store >" ),
    symTrans( Transform::Fairness, "::divine::graph::FairGraph< Graph, Store >" ),
    symTrans( Transform::POR,      "::divine::algorithm::PORGraph< Graph, Store, Stat >" ),

    symVis( Visitor::Partitioned ),
    symVis( Visitor::Shared ),

    symStore( Store::NDFSNTreeStore ),
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

}
}

#endif // DIVINE_INSTANCES_DEFINITIONS
