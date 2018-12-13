#include <lart/abstract/domain.h>

#include <lart/support/util.h>

#include <optional>
#include <iostream>

namespace lart {
namespace abstract {

using namespace llvm;

using lart::util::get_module;

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

Domain domain( MDNode * node ) {
    auto &dom = cast< MDNode >( node->getOperand( 0 ) )->getOperand( 0 );
    return Domain( cast< MDString >( dom )->getString().str() );
}

} // anonymous namespace

void process( StringRef prefix, Module &m ) noexcept {
    auto &ctx = m.getContext();
    MetadataBuilder mdb( ctx );

    for ( const auto &fn : functions_with_prefix( m, prefix ) ) {
        for ( const auto &u : fn->users() ) {
            if ( auto call = dyn_cast< CallInst >( u ) ) {
                auto inst = cast< Instruction >( call->getOperand( 0 )->stripPointerCasts() );
                add_abstract_metadata( inst, Domain( annotation( call ).name() ) );
            }
        }
    }
}

template< typename Value >
void annotation_to_transform_metadata( StringRef anno_namespace, Module &m ) {
    auto &ctx = m.getContext();
    brick::llvm::enumerateAnnosInNs< Value >( anno_namespace, m, [&] ( auto val, auto anno ) {
        auto name = anno_namespace.str() + "." + anno.toString();
        val->setMetadata( name, empty_metadata_tuple( ctx ) );
    });
}
template< typename Value >
void annotation_to_domain_metadata( StringRef anno_namespace, Module &m ) {
    auto &ctx = m.getContext();
    MetadataBuilder mdb( ctx );

    brick::llvm::enumerateAnnosInNs< Value >( anno_namespace, m, [&] ( auto val, auto anno ) {
        auto dn = mdb.domain_node( Domain( anno.name() ) );
        val->setMetadata( anno_namespace, MDTuple::get( ctx, { dn } ) );
    });
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

    annotation_to_domain_metadata< Function >( abstract_tag, m );
    annotation_to_domain_metadata< GlobalVariable >( abstract_domain_tag, m );
    annotation_to_domain_metadata< GlobalVariable >( abstract_domain_kind, m );

    annotation_to_transform_metadata< Function >( "lart.transform", m );

    for ( auto & fn : m ) {
        if ( auto md = fn.getMetadata( abstract_tag ) )
            for ( auto user : fn.users() )
                if ( auto call = dyn_cast< CallInst >( user ) )
                    add_abstract_metadata( call, domain( md ) );
    }
}

inline MDTuple* make_mdtuple( LLVMContext &ctx, unsigned size ) {
    std::vector< Metadata* > doms;
    doms.reserve( size );

    MetadataBuilder mdb( ctx );
    std::generate_n( std::back_inserter( doms ), size,
                     [&]{ return mdb.domain_node( Domain::Concrete() ); } );

    return MDTuple::get( ctx, doms );
}

static const Bimap< DomainKind, std::string > KindTable = {
     { DomainKind::scalar , "scalar"  }
    ,{ DomainKind::pointer, "pointer" }
    ,{ DomainKind::content, "content"  }
    ,{ DomainKind::custom , "custom"  }
};

Domain DomainMetadata::domain() const {
    auto meta = glob->getMetadata( abstract_domain_tag );
    auto &tag = cast< MDNode >( meta->getOperand( 0 ) )->getOperand( 0 );
    return Domain( cast< MDString >( tag )->getString().str() );
}

DomainKind DomainMetadata::kind() const {
    auto meta = glob->getMetadata( abstract_domain_kind );
    auto data = cast< MDTuple >( meta->getOperand( 0 ) );
    return KindTable[ cast< MDString >( data->getOperand( 0 ) )->getString().str() ];
}

Type * DomainMetadata::base_type() const {
    return glob->getType()->getPointerElementType()->getStructElementType( base_type_offset );
}

llvm::Value * DomainMetadata::default_value() const {
    auto type = base_type();
    if ( type->isPointerTy() )
        return ConstantPointerNull::get( cast< PointerType >( type ) );
    if ( type->isIntegerTy() )
        return ConstantInt::get( type, 0 );
    if ( type->isFloatingPointTy() )
        return ConstantFP::get( type, 0 );
    UNREACHABLE( "Unsupported base type." );
}

std::string ValueMetadata::name() const noexcept {
    return _md->getValue()->getName();
}

llvm::Value* ValueMetadata::value() const noexcept {
    return _md->getValue();
}

Domain ValueMetadata::domain() const noexcept {
    auto inst = cast< Instruction >( value() );
    if ( !has_abstract_metadata( inst ) )
        return Domain::Concrete();
    auto md = get_abstract_metadata( inst );
    return ::lart::abstract::domain( md );
}

MDNode* MetadataBuilder::domain_node( Domain dom ) {
    auto name = MDString::get( ctx, dom.name() );
    return MDNode::get( ctx, name );
}

MDNode* MetadataBuilder::domain_node( StringRef dom ) {
    auto name = MDString::get( ctx, dom );
    return MDNode::get( ctx, name );
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
        MetadataBuilder mdb( ctx );
        md->replaceOperandWith( idx, mdb.domain_node( dom ) );
    }
}

Domain FunctionMetadata::get_arg_domain( unsigned idx ) const {
    if ( auto md = fn->getMetadata( tag ) )
        return ArgMetadata( md->getOperand( idx ).get() ).domain();
    return Domain::Concrete();
}

bool FunctionMetadata::has_arg_domain( unsigned idx ) const {
    if ( auto md = fn->getMetadata( tag ) )
        return ArgMetadata( md->getOperand( idx ).get() ).domain() != Domain::Concrete();
    return false;
}

void FunctionMetadata::clear() {
    if ( fn->getMetadata( tag ) )
        fn->setMetadata( tag, nullptr );
}

void set_metadata( llvm::Instruction * inst, const std::string& tag, llvm::Value * value ) {
    auto &ctx = inst->getContext();
    inst->setMetadata( tag, MDTuple::get( ctx, { ValueAsMetadata::get( value ) } ) );
}

llvm::Value * get_metadata( llvm::Instruction * inst, const std::string& tag ) {
    auto &meta = inst->getMetadata( tag )->getOperand( 0 );
    return cast< ValueAsMetadata >( meta.get() )->getValue();
}

void set_addr_offset( llvm::Instruction *inst, llvm::Value * offset ) {
    set_metadata( inst, "lart.addr.offset", offset );
}

void set_addr_origin( llvm::Instruction *inst, llvm::Value * origin ) {
    set_metadata( inst, "lart.addr.origin", origin );
}

llvm::Value * get_addr_offset( llvm::Instruction *inst ) {
    return get_metadata( inst, "lart.addr.offset" );
}

llvm::Value * get_addr_origin( llvm::Instruction *inst ) {
    return get_metadata( inst, "lart.addr.origin" );
}

void make_duals( Instruction *a, Instruction *b ) {
    set_metadata( a, "lart.dual", b );
    set_metadata( b, "lart.dual", a );
}

Value* get_dual( Instruction *inst ) {
    return get_metadata( inst, "lart.dual" );
}

std::vector< ValueMetadata > abstract_metadata( llvm::Module &m ) {
    return query::query( m )
        .map( []( auto &fn ) { return abstract_metadata( &fn ); } )
        .flatten()
        .freeze();
}

std::vector< ValueMetadata > abstract_metadata( llvm::Function *fn ) {
    std::vector< ValueMetadata > mds;
    if ( fn->getMetadata( "lart.abstract.roots" ) ) {
        auto abstract = query::query( *fn ).flatten()
            .map( query::refToPtr )
            .filter( [] ( const auto& inst ) {
                return has_abstract_metadata( inst );
            } )
            .freeze();
        std::move( abstract.begin(), abstract.end(), std::back_inserter( mds ) );
    }
    return mds;
}

bool has_abstract_metadata( llvm::Value *val ) {
    if ( llvm::isa< llvm::Constant >( val ) )
        return false;
    else if ( auto arg = llvm::dyn_cast< llvm::Argument >( val ) )
        return has_abstract_metadata( arg );
    else
        return has_abstract_metadata( llvm::cast< llvm::Instruction >( val ) );
}

bool has_abstract_metadata( llvm::Argument *arg ) {
    return FunctionMetadata( arg->getParent() ).has_arg_domain( arg->getArgNo() );
}

bool has_abstract_metadata( llvm::Instruction *inst ) {
    return inst->getMetadata( "lart.domains" );
}

MDNode * get_abstract_metadata( llvm::Instruction *inst ) {
    ASSERT( has_abstract_metadata( inst ) );
    return cast< MDNode >( inst->getMetadata( "lart.domains" ) );
}

void add_abstract_metadata( llvm::Instruction *inst, Domain dom ) {
    auto& ctx = inst->getContext();
    inst->getFunction()->setMetadata( "lart.abstract.roots", empty_metadata_tuple( ctx ) );
    // TODO enable multiple domains per instruction
    auto node = MetadataBuilder( ctx ).domain_node( dom );
    inst->setMetadata( "lart.domains", MDTuple::get( ctx, node ) );
}

inline bool accessing_abstract_offset( GetElementPtrInst * gep ) {
    return std::any_of( gep->idx_begin(), gep->idx_end(), [] ( const auto & idx ) {
        return has_abstract_metadata( idx );
    } );
}

inline bool allocating_abstract_size( AllocaInst * a ) {
    return has_abstract_metadata( a->getArraySize() );
}

bool forbidden_propagation_by_domain( llvm::Instruction * inst, Domain dom ) {
    auto dm = domain_metadata( *inst->getModule(), dom );

    switch ( dm.kind() ) {
        case DomainKind::scalar:
            if ( auto gep = dyn_cast< GetElementPtrInst >( inst ) ) {
                return accessing_abstract_offset( gep );
            }
            if ( auto a = dyn_cast< AllocaInst >( inst ) ) {
                return allocating_abstract_size( a );
            }
        case DomainKind::content:
        case DomainKind::pointer:
        case DomainKind::custom:
        default:
            return false;
    }
}

bool is_propagable_in_domain( llvm::Instruction *inst, Domain dom ) {
    auto dm = domain_metadata( *inst->getModule(), dom );

    switch ( dm.kind() ) {
        case DomainKind::scalar:
            return is_transformable_in_domain( inst, dom ) ||
                   util::is_one_of< CallInst, StoreInst, GetElementPtrInst,
                                    IntToPtrInst, PtrToIntInst, ReturnInst >( inst );
        case DomainKind::content:
            return is_transformable_in_domain( inst, dom ) ||
                   util::is_one_of< CallInst, ReturnInst >( inst );
        case DomainKind::pointer:
        case DomainKind::custom:
        default:
            UNREACHABLE( "Unsupported domain transformation." );
    }
}

bool is_duplicable( Instruction *inst ) {
    return is_duplicable_in_domain( inst, get_domain( inst ) );
}

bool is_duplicable_in_domain( Instruction *inst, Domain dom ) {
    if ( !is_transformable_in_domain( inst, dom ) )
        return false;

    auto dm = domain_metadata( *inst->getModule(), dom );

    switch ( dm.kind() ) {
        case DomainKind::scalar:
            return true;
        case DomainKind::content:
            return !util::is_one_of< LoadInst, StoreInst, GetElementPtrInst >( inst );
        case DomainKind::pointer:
        case DomainKind::custom:
        default:
            UNREACHABLE( "Unsupported domain transformation." );
    }
}

bool is_transformable( Instruction *inst ) {
    return is_transformable_in_domain( inst, get_domain( inst ) );
}

bool is_transformable_in_domain( llvm::Instruction *inst, Domain dom ) {
    auto m = inst->getModule();
    auto dm = domain_metadata( *m, dom );

    switch ( dm.kind() ) {
        case DomainKind::scalar:
            return is_base_type_in_domain( m, inst, dom ) &&
                   (util::is_one_of< BinaryOperator, CastInst, LoadInst, PHINode >( inst ) ||
                   ( isa< CmpInst >( inst ) && query::query( inst->operands() ).all( [m, dom] ( auto &op ) {
                        return is_base_type_in_domain( m, op, dom );
                   } ) ));
        case DomainKind::content:
            if ( auto call = dyn_cast< CallInst >( inst ) ) {
                if ( auto fn = call->getCalledFunction() ) {
                    auto name =  "__" + dom.name() + "_" + fn->getName().str();
                    return get_module( inst )->getFunction( name );
                }
            }
            return util::is_one_of< LoadInst, StoreInst, GetElementPtrInst >( inst );
        case DomainKind::pointer:
        case DomainKind::custom:
        default:
            UNREACHABLE( "Unsupported domain transformation." );
    }
}

bool is_base_type( llvm::Module *m, llvm::Value * val ) {
    return is_base_type_in_domain( m, val, get_domain( val ) );
}

bool is_base_type_in_domain( llvm::Module *m, llvm::Value * val, Domain dom ) {
    if ( is_concrete( dom ) )
        return true;

    auto type = val->getType();
    auto dm = domain_metadata( *m, dom );
    switch ( dm.kind() ) {
        case DomainKind::scalar:
            return type->isIntegerTy() || type->isFloatingPointTy();
        case DomainKind::content:
        case DomainKind::pointer:
            return type->isPointerTy();
        case DomainKind::custom:
        default:
            UNREACHABLE( "Unsupported domain type." );
    }
}

DomainMetadata domain_metadata( Module &m, Domain dom ) {
    std::optional< DomainMetadata > meta;

    global_variable_walker( m, [&] ( auto glob, auto anno ) {
        if ( anno.name() == dom.name() )
            meta = DomainMetadata( glob );
    } );

    ASSERT( meta && "Domain specification was not found." );
    return meta.value();
}

} // namespace abstract
} // namespace lart


