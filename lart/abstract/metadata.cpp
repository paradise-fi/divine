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

Domain domain( StringRef &ann ) {
    const std::string prefix = "lart.abstract.";
    ASSERT( ann.startswith( prefix ) );

    auto name = ann.str().substr( prefix.size() );
    if ( !DomainTable.count( name ) )
        throw std::runtime_error( "Unknown domain annotation: " + name );
    return DomainTable[ name ];
}

Domain domain( CallInst *call ) {
    auto data = cast< GlobalVariable >( call->getOperand( 1 )->stripPointerCasts() );
    auto ann = cast< ConstantDataArray >( data->getInitializer() )->getAsCString();
    return domain( ann );
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

// CreateAbstractMetadata pass transform annotations into llvm metadata.
//
// As result of the pass, each function with annotated values has
// annotation with name: "lart.abstract.roots".
//
// Where root instructions are marked with MDTuple of domains
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
                    amap[ get_function( inst ) ][ inst ].emplace( domain( call ) );
                }
        }
    }

    MDBuilder mdb( ctx );
    for ( auto &it : amap ) {
        auto &fn = it.first;
        auto &vmap = it.second;

        for ( const auto &av : vmap ) {
            auto &inst = av.first;
            std::vector< Metadata* > doms;

            for ( auto dom : av.second )
                doms.emplace_back( mdb.domain_node( dom ) );

            inst->setMetadata( "lart.domains", MDTuple::get( ctx, doms ) );
        }

        fn->setMetadata( "lart.abstract.roots", MDTuple::get( ctx, {} ) );
    }

    // process annotation of functions
    for ( auto &g : m.globals() ) {
        if ( g.getName() == "llvm.global.annotations" ) {
            auto ca = cast< ConstantArray >( g.getOperand(0) );
            for ( auto &op : ca->operands() ) {
                auto cs = cast< ConstantStruct >( op );
                auto fn = cast< Function >( cs->getOperand(0)->getOperand(0) );
                auto anngl = cast< GlobalVariable >( cs->getOperand(1)->getOperand(0) );
                auto ann = cast< ConstantDataArray >(
                                  anngl->getInitializer())->getAsCString();
                if ( ann.startswith( "lart.abstract" ) ) {
                    auto dom = domain( ann );
                    auto dn = mdb.domain_node( dom );
                    fn->setMetadata( "lart.abstract.return", MDTuple::get( ctx, { dn } ) );
                }
            }
        }
    }
}

// TODO refactore rest of additions of metadata
void add_domain_metadata( Instruction *i, Domain dom ) {
    auto &ctx = i->getContext();
    MDBuilder mdb( ctx );
    std::vector< Metadata* > doms = { mdb.domain_node( dom ) };
    i->setMetadata( "lart.domains", MDTuple::get( ctx, doms ) );
}


void make_duals( Instruction *a, Instruction *b ) {
    auto &ctx = a->getContext();
    a->setMetadata( "lart.dual", MDTuple::get( ctx, { ValueAsMetadata::get( b ) } ) );
    b->setMetadata( "lart.dual", MDTuple::get( ctx, { ValueAsMetadata::get( a ) } ) );
}

bool has_dual( Instruction *inst ) {
    return inst->getMetadata( "lart.dual" );
}

Value* get_dual( Instruction *inst ) {
    auto &dual = inst->getMetadata( "lart.dual" )->getOperand( 0 );
    auto md = cast< ValueAsMetadata >( dual.get() );
    return md->getValue();
}

std::vector< MDValue > abstract_metadata( llvm::Module &m ) {
    return query::query( m )
        .map( []( auto &fn ) { return abstract_metadata( &fn ); } )
        .flatten()
        .freeze();
}

std::vector< MDValue > abstract_metadata( llvm::Function *fn ) {
    std::vector< MDValue > mds;
    if ( fn->getMetadata( "lart.abstract.roots" ) ) {
        auto abstract = query::query( *fn ).flatten()
            .map( query::refToPtr )
            .filter( [] ( auto i ) { return i->getMetadata( "lart.domains" ); } )
            .freeze();
        for ( auto i : abstract )
            mds.emplace_back( i );
    }
    return mds;
}

void dump_abstract_metadata( llvm::Module &m ) {
    for ( auto &fn : m ) {
        auto mds = abstract_metadata( &fn );
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
