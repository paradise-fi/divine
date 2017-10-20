// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/IRBuilder.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/abstract/vpa.h>
#include <lart/abstract/domains/domains.h>
#include <lart/support/util.h>
#include <lart/abstract/util.h>
#include <lart/abstract/intrinsic.h>

namespace lart {
namespace abstract {

namespace {

static inline auto liftsOf( llvm::Instruction * i ) {
    return query::query( llvmFilter< llvm::CallInst >( i->users() ) )
        .filter( []( llvm::CallInst * c ) { return isLift( c ); } ).freeze();
}

static inline std::string prefix( const AbstractValue & av ) {
    auto i = av.get< llvm::Instruction >();
    return "lart." + DomainTable[ av.domain ] + "." + i->getOpcodeName();
}

using Predicate = llvm::CmpInst::Predicate;
const Map< Predicate, std::string > predicate = {
    { Predicate::ICMP_EQ, "eq" },
    { Predicate::ICMP_NE, "ne" },
    { Predicate::ICMP_UGT, "ugt" },
    { Predicate::ICMP_UGE, "uge" },
    { Predicate::ICMP_ULT, "ult" },
    { Predicate::ICMP_ULE, "ule" },
    { Predicate::ICMP_SGT, "sgt" },
    { Predicate::ICMP_SGE, "sge" },
    { Predicate::ICMP_SLT, "slt" },
    { Predicate::ICMP_SLE, "sle" }
};

static std::string name( const AbstractValue & av ) {
    auto pref = prefix( av );
    if ( av.isa< llvm::AllocaInst >() )
        return pref + "." + llvmname( av.value->getType()->getPointerElementType() );
    if ( av.isa< llvm::LoadInst >() )
        return pref + "." + llvmname( av.value->getType() );
    if ( auto s = llvm::dyn_cast< llvm::StoreInst >( av.value ) )
        return pref + "." + llvmname( s->getValueOperand()->getType() );
    if ( auto icmp = llvm::dyn_cast< llvm::ICmpInst >( av.value ) )
        return pref + "_" + predicate.at( icmp->getPredicate() )
                    + "." + llvmname( icmp->getOperand( 0 )->getType() );
    if ( av.isa< llvm::BinaryOperator >() )
        return pref + "." + llvmname( av.value->getType() );
    if ( auto i = llvm::dyn_cast< llvm::CastInst >( av.value ) )
        return pref + "." + llvmname( i->getSrcTy() ) + "." + llvmname( i->getDestTy() );
    UNREACHABLE( "Unhandled intrinsic." );
}

} // anonymous namespace

template< typename VMap, typename TMap, typename Fns >
struct AbstractBuilder {

    using IRB = llvm::IRBuilder<>;

    AbstractBuilder( VMap & vmap, TMap & tmap, Fns & fns )
        : vmap( vmap ), tmap( tmap ), fns( fns ), ibuilder(*this) {}

    void process( const AbstractValue & av ) {
        if ( auto v = av.safeGet< llvm::Instruction >() )
            return process( v, av.domain );
        if ( auto a = av.safeGet< llvm::Argument >() )
            return process( a, av.domain );
        UNREACHABLE( "Trying to process unsupported value.");
    }

    void process( llvm::Argument * arg, Domain dom ) {
        IRB irb( arg->getParent()->front().begin() );
        if ( isAbstract( arg->getType() ) )
            vmap.insert( arg, arg );
        else if ( operatesWithStructTy( arg ) )
            vmap.insert( arg, arg );
        else
            vmap.insert( arg, lift( { arg, dom }, irb ) );
    }

    void process( llvm::Instruction * inst, Domain dom ) {
        if ( isLift( inst ) )
            return; // skip
        if ( auto abstract = create( { inst, dom } ) ) {
            vmap.insert( inst, abstract );
            for ( auto & lift : liftsOf( inst ) )
                lift->replaceAllUsesWith( abstract );
        }
    }

    void processStructOp( const AbstractValue & av ) {
        if ( auto cs = llvm::CallSite( av.value ) ) {
            auto fn = cs.getCalledFunction();
            if ( fn->isIntrinsic() ) {
                createIntrinsicCall( av );
            } else {
                if ( !isIntrinsic( cs.getInstruction() ) )
                    vmap.insert( av.value, createCallSite( av ) );
            }
        } else {
            if ( !isAbstract( av.value->getType() ) && operatesWithStructTy( av.value ) )
                vmap.insert( av.value, av.value );
        }
    }

private:
    llvm::Type * getType( const AbstractValue & av ) const {
        return av.type( tmap );
    }

    decltype(auto) operands( const AbstractValue & av ) {
        if ( auto cs = llvm::CallSite( av.value ) )
            return cs.args();
        else
            return av.get< llvm::Instruction >()->operands();
    }

    Args createArgsOf( const AbstractValue & av, IRB & irb ) {
        return query::query( operands( av ) ).map( [&] ( const auto & op ) {
            return lift( { op.get(), av.domain }, irb );
        } ).freeze();
    }

    bool isAbstract( llvm::Type * t ) const { return tmap.isAbstract( t ); }

    llvm::Instruction * create( const AbstractValue & av ) {
        llvm::Instruction * intr = nullptr;
        llvmcase( av.value,
             [&]( llvm::AllocaInst * ) { intr = createAlloca( av ); }
            ,[&]( llvm::LoadInst * ) { intr = _create( av ); }
            ,[&]( llvm::StoreInst *) { intr = _create( av ); }
            ,[&]( llvm::BinaryOperator * ) { intr = _create( av ); }
            ,[&]( llvm::CastInst * i ) {
                intr = i->getType()->isPointerTy()
                ? createPtrCast( av )
                : _create( av );
            }
            ,[&]( llvm::ICmpInst * ) { intr = _create( av ); }
            ,[&]( llvm::CallInst * i ) {
                auto fn = i->getCalledFunction();
                if ( fn->isIntrinsic() )
                    intr = createIntrinsicCall( av );
                else
                    intr = createCallSite( av );
            }
            ,[&]( llvm::InvokeInst * ) {
                intr = createCallSite( av );
            }
            ,[&]( llvm::PHINode * ) { intr = createPhi( av ); }
            ,[&]( llvm::BranchInst * ) { intr = createBr( av ); }
            ,[&]( llvm::ReturnInst * ) { intr = createRet( av ); }
            ,[&]( llvm::GetElementPtrInst * ) { intr = createGEP( av ); }
            ,[&]( llvm::Instruction * inst ) {
                std::cerr << "ERR: unknown instruction: ";
                inst->dump();
                std::exit( EXIT_FAILURE );
            } );
        return intr;
    }

    llvm::Instruction * _create( const AbstractValue & av ) {
        IRB irb( av.get< llvm::Instruction >() );
        auto args = createArgsOf( av, irb );
        return irb.CreateCall( intrinsic( av, args ), args );
    }

    llvm::Instruction * createICmp( const AbstractValue & av ) {
        IRB irb( av.get< llvm::ICmpInst >() );
        auto args = createArgsOf( av, irb );
        return irb.CreateCall( intrinsic( av, args ), args );
    }


    llvm::Instruction * createAlloca( const AbstractValue & av ) {
        auto a = av.get< llvm::Instruction >();
        return IRB( a ).CreateCall( intrinsic( av, {} ), {} );
    }

    llvm::Instruction * createPtrCast( const AbstractValue & av ) {
        auto i = av.get< llvm::CastInst >();
        assert( llvm::isa< llvm::BitCastInst >( i ) &&
           "ERR: Only bitcast is supported from pointer cast instructions." );

        auto type = getType( av );
        auto l = vmap.lift( i->getOperand( 0 ) );
        auto res = ( l->getType() == type ) ? l : IRB( i ).CreateBitCast( l, type );
        return llvm::cast< llvm::Instruction >( res );
    }

    llvm::Instruction * createIntrinsicCall( const AbstractValue & av ) {
        auto i = av.get< llvm::IntrinsicInst >();
        auto fn = i->getCalledFunction();
        assert( fn->isIntrinsic() );

        auto name = llvm::Intrinsic::getName( i->getIntrinsicID() );

        if ( i->getType()->isPointerTy() ) {
            // skip llvm.ptr.annotation function calls
            assert( fn->getName().startswith( "llvm.ptr.annotation" ) );
            i->replaceAllUsesWith( i->getArgOperand( 0 ) );
        } else if ( name == "llvm.memcpy" ) {

        } else {
            Set< std::string > ignore = {"llvm.lifetime.start"
                                        ,"llvm.lifetime.end"
                                        ,"llvm.var.annotation"
            };
            if ( !ignore.count( name ) ) {
                std::cerr << "ERR: unknown intrinsic: ";
                i->dump();
                std::exit( EXIT_FAILURE );
            }
        }
        return nullptr;
    }

    llvm::FunctionType * intrinsicFTy( const AbstractValue & av, const Args & args ) {
        return llvm::FunctionType::get( getType( av ), typesOf( args ), false );
    }

    llvm::Instruction * createCallSite( const AbstractValue & av ) {
        auto cs = llvm::CallSite( av.value );
        auto i = cs.getInstruction();
        assert( !isIntrinsic( i ) );

        auto args = remapFn( operands( av ), [&] ( const auto & op ) {
            auto l = vmap.safeLift( op.get() );
            return l.isJust() ? l.value() : op.get();
        } );

        auto calle = fns.get( cs.getCalledFunction(), typesOf( args ) );
        IRB irb( i );

        llvm::Instruction * intr = nullptr;
        if ( cs.isCall() ) {
            intr = irb.CreateCall( calle, args );
        } else if ( cs.isInvoke() ) {
            auto inv = llvm::dyn_cast< llvm::InvokeInst >( i );
            intr = irb.CreateInvoke( calle, inv->getNormalDest(), inv->getUnwindDest(), args );
        }
        assert( intr );

        if ( intr->getType() == i->getType() )
            i->replaceAllUsesWith( intr );
        return intr;
    }

    llvm::Instruction * createPhi( const AbstractValue & av ) {
        auto n = av.get< llvm::PHINode >();

        unsigned int niv = n->getNumIncomingValues();
        auto phi = IRB( n ).CreatePHI( getType( av ), niv );
        vmap.insert( n, phi );
        if ( isAbstract( n->getType() ) )
            n->replaceAllUsesWith( phi );

        for ( unsigned int i = 0; i < niv; ++i ) {
            auto val = n->getIncomingValue( i );
            auto parent = n->getIncomingBlock( i );
            if ( vmap.safeLift( val ).isJust() ) {
                phi->addIncoming( vmap.lift( val ), parent );
            } else {
                if ( isAbstract( val->getType() ) )
                    phi->addIncoming( val, parent );
                else {
                    auto term = vmap.safeLift( parent->getTerminator() ).isJust()
                        ? vmap.lift( parent->getTerminator() )
                        : parent->getTerminator();
                    auto nbb =  parent->splitBasicBlock( llvm::cast< llvm::Instruction >( term ) );
                    IRB irb( nbb->begin() );
                    phi->addIncoming( lift( { val, av.domain }, irb ), nbb );
                }
            }
        }
        return phi;
    }

    llvm::Instruction * createBr( const AbstractValue & av ) {
        auto i = av.get< llvm::BranchInst >();
        IRB irb( i );
        if ( i->isUnconditional() ) {
            auto dest = i->getSuccessor( 0 );
            return irb.CreateBr( dest );
        } else {
            llvm::Value * cond;
            if ( vmap.safeLift( i->getCondition() ).isJust() ) {
                auto acond = vmap.lift( i->getCondition() );
                auto tristate = toTristate( acond, av.domain, irb );
                cond = lower( tristate, irb );
            } else {
                cond = i->getCondition();
            }
            return irb.CreateCondBr( cond, i->getSuccessor( 0 ), i->getSuccessor( 1 ) );
        }
    }

    llvm::Instruction * createGEP( const AbstractValue & av ) {
        auto gep = av.get< llvm::GetElementPtrInst >();
        auto bc = llvm::cast< llvm::Instruction >(
                  IRB( gep ).CreateBitCast( gep, getType( av ) ) );
        bc->removeFromParent();
        bc->insertAfter( gep );
        return bc;
    }

    llvm::Instruction * createRet( const AbstractValue & av ) {
        auto i = av.get< llvm::ReturnInst >();
        if ( auto l = vmap.safeLift( i->getReturnValue() ) )
            return IRB( i ).CreateRet( l.value() );
        else
            return IRB( i ).CreateRet( i->getReturnValue() );
    }

    llvm::Value * lift( const AbstractValue & av, IRB & irb ) {
        auto l = vmap.safeLift( av.value );
        if ( l.isJust() ) return l.value();

        auto type = av.value->getType();
        if ( getType( av ) == type )
            return av.value;
        auto fty = llvm::FunctionType::get( getType( av ), { type }, false );
        auto name = "lart." + DomainTable[ av.domain ] + ".lift." + llvmname( type );
        auto fn = irb.GetInsertBlock()->getModule()->getOrInsertFunction( name, fty );
        return irb.CreateCall( fn , av.value );
    }

    llvm::Instruction * lower( llvm::Value * v, IRB & irb ) {
        assert( isAbstract( v->getType() ) );
        auto t = tmap.origin( v->getType() );
        auto fty = llvm::FunctionType::get( t.first, { v->getType() }, false );
        auto name = "lart." + DomainTable[ t.second ] + ".lower";
        if ( t.second != Domain::Tristate )
            name += "." + llvmname( t.first );
        auto fn = irb.GetInsertBlock()->getModule()->getOrInsertFunction( name, fty );
        auto call = irb.CreateCall( fn , v );

        // TODO have to be here?
        vmap.insert( v, call );
        return call;
    }

    llvm::Instruction * toTristate( llvm::Value * v, Domain d, IRB & irb ) {
        assert( isAbstract( v->getType() ) );
        auto o = vmap.origin( v );
        auto tristate = liftType( o->getType(), Domain::Tristate, tmap );
        auto fty = llvm::FunctionType::get( tristate, { v->getType() }, false );
        auto name = "lart." + DomainTable[ d ] + ".bool_to_tristate";
        auto fn = irb.GetInsertBlock()->getModule()->getOrInsertFunction( name, fty );
        return irb.CreateCall( fn , v );
    }

    llvm::Function * intrinsic( const AbstractValue & av, const Args & args ) {
        return ibuilder.make( av, args );
    }

    struct IntrinsicBuilder {
        IntrinsicBuilder( AbstractBuilder & builder ) : builder( builder ) {}

        llvm::Function * make( const AbstractValue & av, const Args & args ) {
            auto k = key( av );
            if ( imap.count( k ) )
                return imap.at( k );
            return imap.emplace( k, make_impl( av, args ) ).first->second;
        }

    private:
        llvm::Function * make_impl( const AbstractValue & av, const Args & args ) {
            auto m = getModule( av.value );
            auto fty = llvm::FunctionType::get( builder.getType( av ), typesOf( args ), false );
            return llvm::cast< llvm::Function >( m->getOrInsertFunction( name( av ), fty ) );
        }

        llvm::Type * intrType( llvm::Instruction * i ) const {
            if ( auto s = llvm::dyn_cast< llvm::StoreInst >( i ) )
                return s->getValueOperand()->getType();
            return i->getType();
        }

        using ICmpKey = std::tuple< Predicate, Domain >;
        ICmpKey icmpKey( llvm::ICmpInst * icmp, Domain dom ) {
            return std::make_tuple( icmp->getPredicate(), dom );
        }


        using OpCode = unsigned;
        using IKey = std::tuple< llvm::Type *, OpCode, Domain >;
        IKey iKey( llvm::Instruction * i, Domain dom ) {
            assert( !llvm::isa< llvm::ICmpInst >( i ) );
            return std::make_tuple( intrType( i ), i->getOpcode(), dom );
        }

        // SrcType, DestType, opcode, domain
        using CastKey = std::tuple< llvm::Type *, llvm::Type *, OpCode, Domain >;
        CastKey castKey( llvm::CastInst * i, Domain dom )  {
            return std::make_tuple( i->getSrcTy(), i->getDestTy(), i->getOpcode(), dom );
        }

        using Key = Union< IKey, ICmpKey, CastKey >;
        Key key( const AbstractValue & av ) {
            if ( auto icmp = av.safeGet< llvm::ICmpInst >() )
                return icmpKey( icmp, av.domain );
            if ( auto cast = av.safeGet< llvm::CastInst >() )
                return castKey( cast, av.domain );
            return iKey( av.get< llvm::Instruction >(), av.domain );
        }

        AbstractBuilder & builder;
        Map< Key, llvm::Function * > imap;
    };

    VMap & vmap;
    TMap & tmap;
    Fns & fns;

    IntrinsicBuilder ibuilder;
};

template< typename VMap, typename TMap, typename Fns >
static auto make_builder( VMap & vmap, TMap & tmap, Fns & fns ) {
    return AbstractBuilder< VMap, TMap, Fns >( vmap, tmap, fns );
}

} // namespace abstract
} // namespace lart
