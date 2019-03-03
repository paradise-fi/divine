#include <lart/abstract/domain.h>

#include <lart/support/util.h>

#include <optional>
#include <iostream>

namespace lart::abstract {

using namespace llvm;

using lart::util::get_module;

    namespace
    {
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

        auto functions_with_prefix( Module &m, StringRef pref ) noexcept {
            auto check = [pref] ( auto fn ) { return fn->getName().startswith( pref ); };
            return query::query( m ).map( query::refToPtr ).filter( check );
        }

        template< typename Yield >
        auto global_variable_walker( llvm::Module &m, Yield yield ) {
            brick::llvm::enumerateAnnosInNs< llvm::GlobalVariable >( meta::tag::domain::name, m, yield );
        }

        Domain domain( llvm::MDNode * node ) { return Domain{ meta::value( node ).value() }; }
    } // anonymous namespace

    void process( StringRef prefix, Module &m ) noexcept {
        for ( const auto &fn : functions_with_prefix( m, prefix ) ) {
            for ( const auto &u : fn->users() ) {
                if ( auto call = dyn_cast< CallInst >( u ) ) {
                    auto inst = call->getOperand( 0 )->stripPointerCasts();
                    meta::abstract::set( inst, annotation( call ).name() );
                }
            }
        }
    }

    template< typename Value >
    void annotation_to_transform_metadata( StringRef anno_namespace, Module &m ) {
        auto &ctx = m.getContext();
        brick::llvm::enumerateAnnosInNs< Value >( anno_namespace, m, [&] ( auto val, auto anno ) {
            auto name = anno_namespace.str() + "." + anno.toString();
            val->setMetadata( name, meta::tuple::empty( ctx ) );
        });
    }
    template< typename Value >
    void annotation_to_domain_metadata( StringRef anno_namespace, Module &m ) {
        auto &ctx = m.getContext();

        brick::llvm::enumerateAnnosInNs< Value >( anno_namespace, m, [&] ( auto val, auto anno ) {
            auto meta = Domain( anno.name() ).meta( ctx );
            val->setMetadata( anno_namespace, meta::tuple::create( ctx, { meta } ) );
        });
    }

    // CreateAbstractMetadata pass transform annotations into llvm metadata.
    //
    // As result of the pass, each function with annotated values has
    // annotation with name: lart::meta::tag::roots.
    //
    // Where root instructions are marked with MDTuple of domains
    // named lart::meta::tag::domains.
    //
    // Domain MDNode holds a string name of domain retrieved from annotation.
    void CreateAbstractMetadata::run( Module &m ) {
        process( "llvm.var.annotation", m );
        process( "llvm.ptr.annotation", m );

        annotation_to_domain_metadata< Function >( meta::tag::abstract, m );
        annotation_to_domain_metadata< GlobalVariable >( meta::tag::domain::name, m );
        annotation_to_domain_metadata< GlobalVariable >( meta::tag::domain::kind, m );

        annotation_to_transform_metadata< Function >( meta::tag::transform::prefix, m );

        for ( auto & fn : m ) {
            if ( auto md = fn.getMetadata( meta::tag::abstract ) )
                for ( auto user : fn.users() )
                    if ( auto call = dyn_cast< CallInst >( user ) )
                        Domain::set( call, domain( md ) );
        }
    }

    static const Bimap< DomainKind, std::string > KindTable = {
         { DomainKind::scalar , "scalar"  }
        ,{ DomainKind::pointer, "pointer" }
        ,{ DomainKind::content, "content"  }
    };

    Domain DomainMetadata::domain() const {
        auto meta = glob->getMetadata( meta::tag::domain::name );
        auto &tag = cast< MDNode >( meta->getOperand( 0 ) )->getOperand( 0 );
        return Domain( cast< MDString >( tag )->getString().str() );
    }

    DomainKind DomainMetadata::kind() const {
        auto meta = glob->getMetadata( meta::tag::domain::kind );
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

    llvm::MDTuple * concrete_domain_tuple( llvm::LLVMContext &ctx, unsigned size ) {
        auto value = [&] { return meta::create( ctx, Domain::Concrete().name() ); };
        return meta::tuple::create( ctx, size, value );
    }

    inline bool accessing_abstract_offset( GetElementPtrInst * gep ) {
        return std::any_of( gep->idx_begin(), gep->idx_end(), [] ( const auto & idx ) {
            return meta::abstract::has( idx.get() );
        } );
    }

    inline bool allocating_abstract_size( AllocaInst * a ) {
        return meta::abstract::has( a->getArraySize() );
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
                       util::is_one_of< AllocaInst, CallInst, ReturnInst >( inst );
            case DomainKind::pointer:
            default:
                UNREACHABLE( "Unsupported domain transformation." );
        }
    }

    bool is_duplicable( Instruction *inst ) {
        return is_duplicable_in_domain( inst, Domain::get( inst ) );
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
            default:
                UNREACHABLE( "Unsupported domain transformation." );
        }
    }

    bool is_transformable( Instruction *inst ) {
        return is_transformable_in_domain( inst, Domain::get( inst ) );
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
            default:
                UNREACHABLE( "Unsupported domain transformation." );
        }
    }

    bool is_base_type( llvm::Module *m, llvm::Value * val ) {
        return is_base_type_in_domain( m, val, Domain::get( val ) );
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
                return type->isPointerTy();
            case DomainKind::pointer:
                return type->isPointerTy();
            default:
                UNREACHABLE( "Unsupported domain type." );
        }
    }

    std::vector< DomainMetadata > domains( llvm::Module & m ) {
        std::vector< DomainMetadata > doms;
        global_variable_walker( m, [&] ( const auto& glob, const auto& ) {
            doms.emplace_back( glob );
        } );
        return doms;
    }

    DomainMetadata domain_metadata( Module &m, const Domain & dom ) {
        auto doms = domains( m );
        auto meta = std::find_if( doms.begin(), doms.end(), [&] ( const auto & data ) {
            return data.domain() == dom;
        } );

        if ( meta != doms.end() )
            return *meta;

        UNREACHABLE( "Domain specification was not found." );
    }

} // namespace lart::abstract


