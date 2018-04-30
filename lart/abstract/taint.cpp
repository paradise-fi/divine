// -*- C++ -*- (c) 2018 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/taint.h>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Argument.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/lib/IR/LLVMContextImpl.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/abstract/metadata.h>
#include <lart/support/util.h>

#include <algorithm>

namespace lart {
namespace abstract {

using namespace llvm;

namespace {

std::string taint_suffix( Type * ret, const Types &args ) {
    std::string res;

    ASSERT( !ret->isVoidTy() );
    if ( auto s = dyn_cast< StructType >( ret ) )
        res += "." + s->getName().str();
    else
        res += "." + llvm_name( ret );

    for ( auto a : args )
        if ( a->isIntegerTy() )
            res += "." + llvm_name( a );
    return res;
}

void use_tainted_value( Instruction *i, Instruction *orig, Instruction *tainted ) {
    if ( auto phi = dyn_cast< PHINode >( i ) ) {
        for ( size_t op = 0; op < phi->getNumIncomingValues(); ++op )
            if ( phi->getIncomingValue( op ) == orig ) {
                phi->setIncomingValue( op, tainted );
                return;
            }
    } else {
        for ( size_t op = 0; op < i->getNumOperands(); ++op ) {
            if ( i->getOperand( op ) == orig ) {
                i->setOperand( op, tainted );
                return;
            }
        }
    }
    UNREACHABLE( "Instruction does not use tainted value." );
}

inline std::string intrinsic_prefix( Instruction *i, Domain d ) {
    return "lart.gen." + DomainTable[ d ] + "." + i->getOpcodeName();
}

using Predicate = CmpInst::Predicate;
const std::unordered_map< Predicate, std::string > predicate = {
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

std::string intrinsic_name( Instruction *i, Domain d ) {
    auto pref = intrinsic_prefix( i, d );

    if ( auto icmp = dyn_cast< ICmpInst >( i ) )
        return pref + "_" + predicate.at( icmp->getPredicate() )
                    + "." + llvm_name( icmp->getOperand( 0 )->getType() );

    if ( isa< BinaryOperator >( i ) )
        return pref + "." + llvm_name( i->getType() );

    if ( auto ci = dyn_cast< CastInst >( i ) )
        return pref + "." + llvm_name( ci->getSrcTy() )
                    + "." + llvm_name( ci->getDestTy() );

    UNREACHABLE( "Unhandled intrinsic." );
}

using AbstractInstBase = std::pair< Instruction*, Domain >;

struct AbstractInst : AbstractInstBase {
    using AbstractInstBase::AbstractInstBase;

    std::string name() {
        return intrinsic_name( instruction(), domain() );
    }

    Types arg_types() {
        auto &ctx = instruction()->getContext();

        Types types;
        for ( auto &a : types_of( instruction()->operands() ) ) {
            types.push_back( Type::getInt1Ty( ctx ) );
            types.push_back( a );
            types.push_back( Type::getInt1Ty( ctx ) );
            types.push_back( abstract_type( a, domain() ) );
        }

        return types;
    }

    Type* type() { return abstract_type( instruction()->getType(), domain() ); }

    Function* get() {
        auto m = get_module( instruction() );
        auto fty = FunctionType::get( type(), arg_types(), false );
        return cast< Function >( m->getOrInsertFunction( name(), fty ) );
    }

    Instruction* instruction() { return first; }
    Domain domain() { return second; }
};

Function* intrinsic( Instruction *i ) {
    auto dom = MDValue( i ).domain();
    return AbstractInst( i, dom ).get();
}

bool is_taintable( Value *v ) {
    return is_one_of< BinaryOperator, CmpInst, TruncInst, SExtInst, ZExtInst >( v );
}

template< typename Args >
Instruction* generate_taint( IRBuilder<> irb, Function *fn, Value *def, const Args &args ) {
    Values targs;

    targs.emplace_back( fn );
    targs.emplace_back( def );
    targs.insert( targs.end(), args.begin(), args.end() );

    auto rty = targs[ 1 ]->getType();
    auto m = fn->getParent();
    auto taint_fn = get_taint_fn( m, rty, types_of( targs ) );

    return cast< Instruction >( irb.CreateCall( taint_fn, targs ) );
}

struct TaintBase {
    virtual Function* intrinsic() = 0;
    virtual Value* default_val() = 0;
    virtual Values args() = 0;
};

struct Taint : TaintBase {
    Taint( Instruction *v )
        : value( v ), dual( cast< Instruction >( get_dual( v ) ) ), irb( dual )
    {}

    Instruction* generate() {
        return generate_taint( irb, intrinsic(), default_val(), args() );
    }

    Function* intrinsic() override {
        using ::lart::abstract::intrinsic;
        return intrinsic( value );
    }

    Value* default_val() override {
        return UndefValue::get( rty() );
    }

    Instruction* placeholder( Argument *arg ) {
        for ( auto u : arg->users() )
            if ( auto call = dyn_cast< CallInst >( u ) )
                if ( call->getCalledFunction()->getName().startswith( "lart.placeholder" ) )
                    return call;
        return nullptr;
    }

    Values args() override {
        Values vals;
        auto dom = MDValue( value ).domain();

        for ( auto & a : value->operands() ) {
            vals.push_back( a );
            auto inst = dyn_cast< Instruction >( a );
            if ( inst && has_dual( inst ) ) {
                vals.push_back( get_dual( inst ) );
            } else {
                if ( auto arg = dyn_cast< Argument >( a ) ) {
                    if ( auto ph = placeholder( arg ) ) {
                        vals.push_back( ph );
                        continue;
                    }
                }

                auto ty = abstract_type( a->getType(), dom );
                vals.push_back( UndefValue::get( ty ) );
            }
        }

        return vals;
    }


protected:
    virtual Type* rty() { return dual->getType(); }

    Instruction *value;
    Instruction *dual;
    IRBuilder<> irb;
};

struct RepTaint : Taint {
    using Taint::Taint;

    Function* intrinsic() final {
        auto m = get_module( value );
        auto i1 = Type::getInt1Ty( value->getContext() );
        auto ty = value->getType();
        auto dom = MDValue( value ).domain();
        auto aty = abstract_type( ty, dom );
        auto addr = cast< LoadInst >( value )->getPointerOperand();

        auto fty = FunctionType::get( aty, { i1, ty, i1, addr->getType() }, false );
        auto name = "lart.gen." + DomainTable[ dom ] + ".rep." + llvm_name( ty );
        return get_or_insert_function( m, fty, name );
    }

    Values args() final {
        auto load = cast< LoadInst >( value );
        return { value, load->getPointerOperand() };
    }
};

struct ToBoolTaint : Taint {
    using Taint::Taint;

    Function* intrinsic() final {
        auto m = get_module( value );
        auto i1 = Type::getInt1Ty( value->getContext() );
        auto dom = MDValue( value ).domain();
        auto ty = value->getType();
        auto aty = dual->getType();

        auto fty = FunctionType::get( i1, { i1, ty, i1, aty }, false );
        auto name = "lart.gen." + DomainTable[ dom ] + ".to_i1";
        return get_or_insert_function( m, fty, name );
    }

    Value* default_val() final { return value; }

    Values args() final { return { value, dual }; }

    Type* rty() override { return value->getType(); }
};

} // anonymous namespace

Function* get_taint_fn( Module *m, Type *ret, Types args ) {
    auto name = "__vm_test_taint" + taint_suffix( ret, args );

    auto fty = FunctionType::get( ret, args, false );
    auto fn = m->getOrInsertFunction( name, fty );
    return cast< Function >( fn );
}

Instruction* create_taint( Instruction *i, const Values &args ) {
    IRBuilder<> irb( i );
	auto rty = args[ 1 ]->getType();
    auto fn = get_taint_fn( get_module( i ), rty, types_of( args ) );

    auto call = irb.CreateCall( fn, args );
    call->removeFromParent();
    call->insertAfter( i );
    call->setMetadata( "lart.domains", i->getMetadata( "lart.domains" ) );
    return call;
}

void Tainting::run( Module &m ) {
    auto abstract = abstract_metadata( m );
    for ( const auto & mdv : abstract )
    	if ( mdv.value()->getType()->isIntegerTy() )
    	    taint( cast< Instruction >( mdv.value() ) );
}

void Tainting::taint( Instruction *i ) {
    if ( has_dual( i ) ) {
        auto taint = [i] () -> Instruction* {
            if ( isa< LoadInst >( i ) )
                return RepTaint( i ).generate();
            if ( isa< PHINode >( i ) )
                return nullptr; // do not taint phinodes
            if ( isa< CallInst >( i ) )
                return nullptr; // skip, calls have been already processed
            if ( is_taintable( i ) )
                return Taint( i ).generate();

            i->dump();
            UNREACHABLE( "Unknown dual instruction" );
        } ();

        if ( auto phi = dyn_cast< PHINode >( i ) ) {
            auto dphi = cast< PHINode >( get_dual( phi ) );
            for ( size_t op = 0; op < phi->getNumIncomingValues(); ++op ) {
                auto in = phi->getIncomingValue( op );
                Value* val = UndefValue::get( dphi->getType() );
                if ( auto iin = dyn_cast< Instruction >( in ) )
                    if ( has_dual( iin ) )
                        val = get_dual( iin );
                dphi->addIncoming( val, phi->getIncomingBlock( op ) );
            }
        }

        if ( taint ) {
            auto dual = cast< Instruction >( get_dual( i ) );
            dual->replaceAllUsesWith( taint );
            dual->eraseFromParent();
            make_duals( i, taint );
        }
    }
}

void TaintBranching::run( Module &m ) {
    for ( auto t : taints( m ) ) {
        auto call = cast< CallInst >( t );
        for ( auto u : get_dual( call )->users() ) {
            if ( auto br = dyn_cast< BranchInst >( u ) )
                expand( call, br );
        }
    }
}

void TaintBranching::expand( CallInst *taint, BranchInst *br ) {
    auto i1 = ToBoolTaint( cast< Instruction >( get_dual( taint ) ) ).generate();
    i1->removeFromParent();
    i1->insertAfter( taint );
    br->setCondition( i1 );
}

bool is_assume_taint( CallInst *taint ) {
    auto fn = taint->getOperand( 0 );
    return fn->getName().count( ".assume" );
}

void LifterSynthesize::run( Module &m ) {
    for ( auto t : taints( m ) ) {
        auto call = cast< CallInst >( t );
        auto fn = cast< Function >( call->getOperand( 0 ) );
        if ( fn->empty() ) {
            if ( is_assume_taint( call ) )
                process_assume( call );
            else
                process( call );
        }
    }
}

void LifterSynthesize::process( CallInst *taint ) {
    auto fn = cast< Function >( taint->getOperand( 0 ) );

    std::vector< LifterArg > args;
    for ( auto it = fn->arg_begin(); it != fn->arg_end(); std::advance( it, 2 ) )
        args.emplace_back( it, std::next( it ) );

    if ( args.size() == 1 )
        generate_unary_lifter( taint, fn, args[ 0 ].second );
    else
        generate_nary_lifter( taint, fn, args );
}

void LifterSynthesize::process_assume( CallInst *taint ) {
    auto fn = cast< Function >( taint->getOperand( 0 ) );
    auto dom = MDValue( taint->getOperand( 1 ) ).domain();

	auto arg = std::next( fn->arg_begin() );
	auto ebb = make_bb( fn, "entry" );

    auto aty = abstract_type( arg->getType(), dom );
	auto cs = Constant::getNullValue( aty );

	IRBuilder<> irb( ebb );
	auto iv = cast< Instruction >( irb.CreateInsertValue( cs, arg, 0 ) );
	auto ar = create_call( irb, rep( arg, dom ), { iv }, dom );
    iv->setMetadata( "lart.domains", ar->getMetadata( "lart.domains" ) );

    Values args = { ar, std::next( fn->arg_begin(), 3 ) };

	auto aop = make_abstract_op( taint, types_of( args ) );
	auto icall = create_call( irb, aop, args, dom );
    auto to = fn->getReturnType();
    auto ur = create_call( irb, unrep( icall, dom, to ), { icall }, dom );
    auto ev = irb.CreateExtractValue( ur, 0 );
    irb.CreateRet( ev );
}

} // namespace abstract
} // namespace lart
