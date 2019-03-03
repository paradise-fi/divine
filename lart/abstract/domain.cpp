#include <lart/abstract/domain.h>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/IRBuilder.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/support/util.h>

#include <optional>
#include <iostream>

namespace lart::abstract {

using namespace llvm;

using lart::util::get_module;

    namespace
    {
        bool accessing_abstract_offset( llvm::GetElementPtrInst * gep ) {
            return std::any_of( gep->idx_begin(), gep->idx_end(), [] ( const auto & idx ) {
                return meta::abstract::has( idx.get() );
            } );
        }

        bool allocating_abstract_size( llvm::AllocaInst * a ) {
            return meta::abstract::has( a->getArraySize() );
        }

        template< typename Yield >
        auto global_variable_walker( llvm::Module &m, Yield yield ) {
            brick::llvm::enumerateAnnosInNs< llvm::GlobalVariable >( meta::tag::domain::name, m, yield );
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

    } // anonymous namespace

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
        meta::create_from_annotation( "llvm.var.annotation", m );
        meta::create_from_annotation( "llvm.ptr.annotation", m );

        annotation_to_domain_metadata< Function >( meta::tag::abstract, m );
        annotation_to_domain_metadata< GlobalVariable >( meta::tag::domain::name, m );
        annotation_to_domain_metadata< GlobalVariable >( meta::tag::domain::kind, m );

        annotation_to_transform_metadata< Function >( meta::tag::transform::prefix, m );

        for ( auto & fn : m ) {
            if ( auto meta = meta::get( &fn, meta::tag::abstract ) ) {
                for ( auto user : fn.users() ) {
                    llvm::Value * val = user;
                    if ( auto ce = llvm::dyn_cast< llvm::ConstantExpr >( user ) )
                        val = lower_constant_expr_call( ce );
                    if ( !val )
                        continue;
                    if ( auto call = llvm::dyn_cast< llvm::CallInst >( val ) )
                        Domain::set( call, Domain{ meta.value() } );
                }
            }
        }
    }

    static const Bimap< DomainKind, std::string > KindTable = {
         { DomainKind::scalar , "scalar"  }
        ,{ DomainKind::pointer, "pointer" }
        ,{ DomainKind::content, "content"  }
    };

    Domain DomainMetadata::domain() const {
        auto meta = meta::get( glob, meta::tag::domain::name );
        return Domain{ meta.value() };
    }

    DomainKind DomainMetadata::kind() const {
        auto meta = meta::get( glob, meta::tag::domain::kind );
        return KindTable[ meta.value() ];
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

    bool forbidden_propagation_by_domain( llvm::Instruction * inst, Domain dom ) {
        auto dm = DomainMetadata::get( inst->getModule(), dom );

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
        auto dm = DomainMetadata::get( inst->getModule(), dom );

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

    bool is_transformable( Instruction *inst ) {
        return is_transformable_in_domain( inst, Domain::get( inst ) );
    }

    bool is_transformable_in_domain( llvm::Instruction *inst, Domain dom ) {
        auto m = inst->getModule();
        auto dm = DomainMetadata::get( m, dom );

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
        auto dm = DomainMetadata::get( m, dom );
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

    llvm::Value * lower_constant_expr_call( llvm::ConstantExpr * ce ) {
        if ( ce->getNumUses() > 0 ) {
            if ( auto orig = llvm::dyn_cast< llvm::CallInst >( *ce->user_begin() ) ) {
                auto fn = ce->getOperand( 0 );
                llvm::IRBuilder<> irb( orig );
                auto call = irb.CreateCall( fn );
                if ( call->getType() != orig->getType() ) {
                    auto cast = irb.CreateBitCast( call, orig->getType() );
                    orig->replaceAllUsesWith( cast );
                } else {
                    orig->replaceAllUsesWith( call );
                }

                orig->eraseFromParent();
                return call;
            }
        }

        return nullptr;
    }

    std::vector< DomainMetadata > domains( llvm::Module & m ) {
        std::vector< DomainMetadata > doms;
        global_variable_walker( m, [&] ( const auto& glob, const auto& ) {
            doms.emplace_back( glob );
        } );
        return doms;
    }

} // namespace lart::abstract


