#include <lart/abstract/metadata.h>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/abstract/util.h>
#include <lart/support/query.h>

#include <brick-llvm>

namespace lart {
namespace abstract {

using namespace llvm;

namespace {

inline auto empty_metadata_tuple( LLVMContext &ctx ) { return MDTuple::get( ctx, {} ); }

inline auto annotation( CallInst *call ) -> brick::llvm::Annotation {
   auto anno = brick::llvm::transformer( call ).operand( 1 )
               .apply( [] ( auto val ) { return val->stripPointerCasts(); } )
               .cast< GlobalVariable >()
               .apply( [] ( auto val ) { return val->getInitializer(); } )
               .cast< ConstantDataArray >()
               .freeze();

    ASSERT( anno && "Call does not have annotation." );
    return brick::llvm::Annotation{ anno->getAsCString() };
}

decltype(auto) functions_with_prefix( Module &m, StringRef pref ) noexcept {
    return query::query( m ).map( query::refToPtr )
          .filter( [pref] ( auto fn ) { return fn->getName().startswith( pref ); } );
}

} // anonymous namespace

void process( StringRef prefix, Module &m ) noexcept {
    auto &ctx = m.getContext();
    MDBuilder mdb( ctx );

    for ( const auto &fn : functions_with_prefix( m, prefix ) ) {
        for ( const auto &u : fn->users() ) {
            if ( auto call = dyn_cast< CallInst >( u ) ) {
                auto inst = cast< Instruction >( call->getOperand( 0 )->stripPointerCasts() );
                add_abstract_metadata( inst, Domain( annotation( call ).name() ) );
            }
        }
    }
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
    process( "llvm.var.annotation", m );
    process( "llvm.ptr.annotation", m );

    auto &ctx = m.getContext();
    MDBuilder mdb( ctx );

    brick::llvm::enumerateFunctionAnnosInNs( "lart.abstract", m, [&] ( auto fn, auto anno ) {
        auto dn = mdb.domain_node( Domain( anno.name() ) );
        fn->setMetadata( "lart.abstract.return", MDTuple::get( ctx, { dn } ) );
    });
}


inline MDTuple* make_mdtuple( LLVMContext &ctx, unsigned size ) {
    std::vector< Metadata* > doms;
    doms.reserve( size );

    MDBuilder mdb( ctx );
    std::generate_n( std::back_inserter( doms ), size,
                     [&]{ return mdb.domain_node( Domain::Concrete() ); } );

    return MDTuple::get( ctx, doms );
}


MDNode* MDBuilder::domain_node( Domain dom ) {
    auto name = MDString::get( ctx, dom.name() );
    return MDNode::get( ctx, name );
}

MDNode* MDBuilder::domain_node( StringRef dom ) {
    auto name = MDString::get( ctx, dom );
    return MDNode::get( ctx, name );
}

std::string MDValue::name() const noexcept {
    return _md->getValue()->getName();
}

llvm::Value* MDValue::value() const noexcept {
    return _md->getValue();
}

Domain MDValue::domain() const noexcept {
    auto inst = cast< Instruction >( value() );
    if ( !inst->getMetadata( "lart.domains" ) )
        return Domain::Concrete();
    auto &dom = cast< MDNode >( inst->getMetadata( "lart.domains" )->getOperand( 0 ) )->getOperand( 0 );
    return Domain( cast< MDString >( dom )->getString().str() );
}

Domain ArgMetadata::domain() const {
    auto mdstr = cast< MDString >( data->getOperand( 0 ) );
    return Domain( mdstr->getString().str() );
}

constexpr char FunctionMetadata::tag[];

void FunctionMetadata::set_arg_domain( unsigned idx, Domain dom ) {
    auto &ctx = fn->getContext();
    auto size = fn->arg_size();

    if ( !fn->getMetadata( tag ) )
        fn->setMetadata( tag, make_mdtuple( ctx, size ) );

    auto md = fn->getMetadata( tag );

    auto curr = get_arg_domain( idx );
    ASSERT( curr == Domain::Concrete() || curr == dom ); // multiple domains are not supported yet

    if ( curr != dom ) {
        MDBuilder mdb( ctx );
        md->replaceOperandWith( idx, mdb.domain_node( dom ) );
    }
}

Domain FunctionMetadata::get_arg_domain( unsigned idx ) {
    if ( auto md = fn->getMetadata( tag ) )
        return ArgMetadata( md->getOperand( idx ).get() ).domain();
    return Domain::Concrete();
}

void FunctionMetadata::clear() {
    if ( fn->getMetadata( tag ) )
        fn->setMetadata( tag, nullptr );
}

void make_duals( Instruction *a, Instruction *b ) {
    auto &ctx = a->getContext();
    a->setMetadata( "lart.dual", MDTuple::get( ctx, { ValueAsMetadata::get( b ) } ) );
    b->setMetadata( "lart.dual", MDTuple::get( ctx, { ValueAsMetadata::get( a ) } ) );
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


void add_abstract_metadata( llvm::Instruction *inst, Domain dom ) {
    auto& ctx = inst->getContext();
    get_function( inst )->setMetadata( "lart.abstract.roots", empty_metadata_tuple( ctx ) );
    // TODO enable multiple domains per instruction
    auto node = MDBuilder( ctx ).domain_node( dom );
    inst->setMetadata( "lart.domains", MDTuple::get( ctx, node ) );
}

} // namespace abstract
} // namespace lart
