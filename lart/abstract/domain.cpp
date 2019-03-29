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
        auto specification_function_walker( llvm::Module &m, Yield yield ) {
            brick::llvm::enumerateAnnosInNs< llvm::Function >( meta::tag::domain::kind, m, yield );
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
        annotation_to_domain_metadata< Function >( meta::tag::domain::spec, m );
        annotation_to_domain_metadata< Function >( meta::tag::domain::kind, m );

        annotation_to_transform_metadata< Function >( meta::tag::transform::prefix, m );

        for ( auto & fn : m ) {
            if ( meta::get( &fn, meta::tag::domain::spec ) )
                continue;
            if ( auto meta = meta::get( &fn, meta::tag::abstract ) ) {
                for ( auto user : fn.users() ) {
                    llvm::Value * val = user;
                    if ( auto ce = llvm::dyn_cast< llvm::ConstantExpr >( user ) )
                        val = lower_constant_expr_call( ce );
                    if ( !val )
                        continue;
                    if ( auto call = llvm::dyn_cast< llvm::CallInst >( val ) ) {
                        meta::set( &fn, meta::tag::abstract_return );
                        meta::set( call, meta::tag::abstract_return );
                        Domain::set( call, Domain{ meta.value() } );
                    }
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
        auto meta = meta::get( spec, meta::tag::domain::spec );
        return Domain{ meta.value() };
    }

    DomainKind DomainMetadata::kind() const {
        auto meta = meta::get( spec, meta::tag::domain::kind );
        return KindTable[ meta.value() ];
    }

    Type * DomainMetadata::base_type() const {
        return spec->getReturnType();
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
        auto m = inst->getModule();
        if ( is_transformable_in_domain( inst, dom ) && is_base_type_in_domain( m, inst, dom ) )
            return true;

        auto dm = DomainMetadata::get( inst->getModule(), dom );

        switch ( dm.kind() ) {
            case DomainKind::scalar:
                return util::is_one_of< AllocaInst, CallInst, StoreInst, LoadInst, CastInst,
                    PHINode, GetElementPtrInst, IntToPtrInst, PtrToIntInst, ReturnInst >( inst );
            case DomainKind::content:
                if ( auto call = llvm::dyn_cast< llvm::CallInst >( inst ) )
                    return is_base_type_in_domain( m, inst, dom );
                return util::is_one_of< AllocaInst, ReturnInst, CastInst >( inst );
            case DomainKind::pointer:
            default:
                UNREACHABLE( "Unsupported domain transformation." );
        }
    }

    bool is_transformable( Instruction *inst ) {
        if ( auto dom = Domain::get( inst ); dom != Domain::Concrete() )
            return is_transformable_in_domain( inst, dom );
        return false;
    }

    bool is_transformable_in_domain( llvm::Instruction *inst, Domain dom ) {
        auto m = inst->getModule();
        auto dm = DomainMetadata::get( m, dom );

        auto in_domain = [&] ( auto vals ) {
            return query::query( vals ).all( [m, dom] ( auto & val ) {
                return is_base_type_in_domain( m, val, dom );
            } );
        };

        if ( auto call = llvm::dyn_cast< llvm::CallInst >( inst ) ) {
            std::string name = "__" + dom.name() + "_";
            if ( llvm::isa< llvm::MemSetInst >( call ) )
                name += "memset";
            else if ( llvm::isa< llvm::MemCpyInst >( call ) )
                name += "memcpy";
            else if ( llvm::isa< llvm::MemMoveInst >( call ) )
                name += "memmove";
            else if ( auto fn = call->getCalledFunction() ) {
                name += fn->getName().str();
            }
            return inst->getModule()->getFunction( name );
        }

        switch ( dm.kind() ) {
            case DomainKind::scalar:
                if ( is_base_type_in_domain( m, inst, dom ) ) {
                    if ( util::is_one_of< BinaryOperator, CastInst, LoadInst, PHINode >( inst ) )
                        return true;
                    if ( isa< CmpInst >( inst ) && in_domain( inst->operands() ) )
                        return true;
                }

                if ( auto store = llvm::dyn_cast< llvm::StoreInst >( inst ) )
                    return is_base_type_in_domain( m, store->getValueOperand(), dom );

                return false;
            case DomainKind::content:
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
        specification_function_walker( m, [&] ( const auto& fn, const auto& ) {
            doms.emplace_back( fn );
        } );
        return doms;
    }

namespace meta {
    void set_value_as_meta( llvm::Instruction * inst, const std::string & tag, llvm::Value * val ) {
        auto &ctx = inst->getContext();

        auto meta = [&] {
            if ( llvm::isa< llvm::Instruction >( val ) )
                return meta::tuple::create( ctx, { llvm::ValueAsMetadata::get( val ) } );
            if ( auto arg = llvm::dyn_cast< llvm::Argument >( val ) ) {
                auto i64 = llvm::Type::getInt64Ty( ctx );
                auto con = llvm::ConstantInt::get( i64, arg->getArgNo() );
                return meta::tuple::create( ctx, { llvm::ConstantAsMetadata::get( con ) } );
            }
            UNREACHABLE( "Unsupported value" );
        } ();

        inst->setMetadata( tag, meta );
    }

    void make_duals( llvm::Value * a, llvm::Instruction * b ) {
        if ( auto arg = llvm::dyn_cast< llvm::Argument >( a ) )
            return make_duals( arg, b );
        if ( auto inst = llvm::dyn_cast< llvm::Instruction >( a ) )
            return make_duals( inst, b );
        UNREACHABLE( "Unsupported dual pair" );
    }

    void make_duals( llvm::Argument * arg, llvm::Instruction * inst ) {
        ASSERT( arg->getParent() == inst->getFunction() );
        set_value_as_meta( inst, meta::tag::dual, arg );
    }

    void make_duals( llvm::Instruction * a, llvm::Instruction * b ) {
        ASSERT( a->getFunction() == b->getFunction() );
        set_value_as_meta( a, meta::tag::dual, b );
        set_value_as_meta( b, meta::tag::dual, a );
    }

    bool has_dual( llvm::Value * val ) {
        if ( llvm::isa< llvm::Constant >( val ) )
            return false;
        if ( auto arg = llvm::dyn_cast< llvm::Argument >( val ) )
            return has_dual( arg );
        if ( auto inst = llvm::dyn_cast< llvm::Instruction >( val ) )
            return has_dual( inst );
        UNREACHABLE( "Unsupported dual value" );
    }

    bool has_dual( llvm::Argument * arg ) {
        return meta::get_dual( arg );
    }

    bool has_dual( llvm::Instruction * inst ) {
        return inst->getMetadata( meta::tag::dual );
    }

    llvm::Value * get_dual( llvm::Value * val ) {
        if ( auto arg = llvm::dyn_cast< llvm::Argument >( val ) )
            return get_dual( arg );
        if ( auto inst = llvm::dyn_cast< llvm::Instruction >( val ) )
            return get_dual( inst );
        UNREACHABLE( "Unsupported dual value" );
    }

    llvm::Value * get_dual( llvm::Argument * arg ) {
        // TODO rework
        auto fn = arg->getParent();
        auto point = fn->getEntryBlock().getFirstNonPHIOrDbgOrLifetime();
        if ( auto call = llvm::dyn_cast< llvm::CallInst >( point ) ) {
            if ( call->getCalledFunction()->getName() == "__lart_unstash" ) {
                auto ptr = llvm::cast< llvm::IntToPtrInst >( *call->user_begin() );
                auto pack = llvm::cast< llvm::LoadInst >( *ptr->user_begin() );
                for ( auto u : pack->users() ) {
                    if ( meta::get_dual( u ) == arg ) {
                        return u;
                    }
                }
            }
        }

        for ( auto u : arg->users() ) {
            if ( auto i = llvm::dyn_cast< llvm::Instruction >( u ); meta::has_dual( i ) ) {
                if ( meta::get_dual( i ) == arg ) {
                    return i; // unstash placeholder
                }
            }
        }

        return nullptr;
    }

    llvm::Value * get_dual( llvm::Instruction *inst ) {
        return get_value_from_meta( inst, meta::tag::dual );
    }
} // namespace meta

} // namespace lart::abstract


