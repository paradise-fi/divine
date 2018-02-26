#include <lart/abstract/metadata.h>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/abstract/util.h>
#include <lart/support/query.h>

namespace lart {
namespace abstract {

using namespace llvm;

Domain domain( CallInst *call ) {
    auto data = cast< GlobalVariable >( call->getOperand( 1 )->stripPointerCasts() );
    auto annotation = cast< ConstantDataArray >( data->getInitializer() )->getAsCString();
    const std::string prefix = "lart.abstract.";
    ASSERT( annotation.startswith( prefix ) );

    auto name = annotation.str().substr( prefix.size() );
    if ( !DomainTable.count( name ) )
        throw std::runtime_error( "Unknown domain annotation: " + name );
    return DomainTable[ name ];
}


MDNode* MDBuilder::domain_node( Domain dom ) {
    auto name = MDString::get( ctx, DomainTable[ dom ] );
    return MDNode::get( ctx, name );
}

std::string MDValue::name() const {
    return _md->getValue()->getName();
}

llvm::Value* MDValue::value() const {
    return _md->getValue();
}

std::vector< Domain > MDValue::domains() const {
    auto inst = cast< Instruction >( _md->getValue() );
    std::vector< Domain > doms;

    for ( auto & dom : inst->getMetadata( "lart.domains" )->operands() ) {
        auto &n = cast< MDNode >( dom.get() )->getOperand( 0 );
        auto dom_name = cast< MDString >( n )->getString().str();
        doms.push_back( DomainTable[ dom_name ] );
    }

    return doms;
}

Domain MDValue::domain() const {
    auto doms = domains();
    ASSERT_EQ( doms.size(), 1 );
    return doms[ 0 ];
}

AbstractValue MDValue::abstract_value() const {
    return AbstractValue( value(), domain() );
}

// CreateAbstractMetadata pass transform annotations into llvm metadata.
//
// As result of the pass, each function with annotated values has MDNode
// with tuple of abstract values, of name: "lart.abstract.values".
//
// Each of the abstract values contains an MDTuple of domains
// named "lart.domains".
//
// Domain MDNode holds a string name of domain retrieved from annotation.
void CreateAbstractMetadata::run( Module &m ) {
    std::vector< std::string > prefixes = {
        "llvm.var.annotation",
        "llvm.ptr.annotation"
    };

    auto &ctx = m.getContext();

    std::map< Function*, std::map< Instruction*, std::set< Domain > > > amap;

    for ( const auto &pref : prefixes ) {
        auto annotated = query::query( m )
            .map( query::refToPtr )
            .filter( [&]( Function *fn ) {
                return fn->getName().startswith( pref );
            } ).freeze();

        for ( const auto &a : annotated ) {
            for ( const auto &u : a->users() )
                if ( auto call = dyn_cast< CallInst >( u ) ) {
                    auto inst = cast< Instruction >( call->getOperand( 0 )->stripPointerCasts() );
                    amap[ getFunction( inst ) ][ inst ].emplace( domain( call ) );
                }
        }
    }

    MDBuilder mdb( ctx );
    for ( auto &it : amap ) {
        auto &fn = it.first;
        auto &vmap = it.second;

        std::vector< Metadata * > vals;
        for ( const auto &av : vmap ) {
            auto &inst = av.first;
            auto &doms = av.second;
            std::vector< Metadata* > mddoms;

            std::transform( doms.begin(), doms.end(), std::back_inserter( mddoms ),
                [&] ( auto dom ) { return mdb.domain_node( dom ); }
            );

            inst->setMetadata( "lart.domains", MDTuple::get( ctx, mddoms ) );
            vals.emplace_back( ValueAsMetadata::get( inst ) );
        }

        fn->setMetadata( "lart.abstract.values", MDTuple::get( ctx, vals ) );
    }
}

std::vector< MDValue > abstract_metadata( const llvm::Module &m ) {
    std::vector< MDValue > mds;
    for ( auto &fn : m ) {
        auto fnmd = abstract_metadata( fn );
        mds.insert( mds.end(), fnmd.begin(), fnmd.end() );
    }
    return mds;
}

std::vector< MDValue > abstract_metadata( const llvm::Function &fn ) {
    std::vector< MDValue > mds;
    if ( auto vals = fn.getMetadata( "lart.abstract.values" ) )
        for ( auto & val : vals->operands() )
            mds.emplace_back( val.get() );
    return mds;
}

void dump_abstract_metadata( const llvm::Module &m ) {
    for ( auto &fn : m ) {
        auto mds = abstract_metadata( fn );
        if ( !mds.empty() ) {
            std::cerr << "\nfunction: " << fn.getName().str() << std::endl;

            for ( auto & mdv : mds ) {
                std::cerr << mdv.name() << " [";
                bool first = true;
                for ( auto dom : mdv.domains() ) {
                    if ( !first )
                        std::cerr << ", ";
                    std::cerr << DomainTable[ dom ];
                    first = false;
                }
                std::cerr << ']' << std::endl;
            }
        }
    }
}

} // namespace abstract
} // namespace lart
