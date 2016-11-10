// -*- C++ -*- (c) 2015 Vladimír Štill <xstill@fi.muni.cz>

DIVINE_RELAX_WARNINGS
#include <llvm/Pass.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/CallSite.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
#include <llvm/Transforms/Utils/Cloning.h>
DIVINE_UNRELAX_WARNINGS
#include <brick-string>
#include <unordered_set>
#include <string>
#include <iostream>

#include <lart/support/pass.h>
#include <lart/support/meta.h>
#include <lart/support/query.h>
#include <lart/support/util.h>

namespace lart {
namespace svcomp {

#ifndef NDEBUG
#define LART_DEBUG( X ) X
#else
#define LART_DEBUG( X ) ((void)0)
#endif

struct NondetTracking : lart::Pass {

    enum class Signed { Unknown = -1, No = 0, Yes = 1 };

    static PassMeta meta() {
        return passMeta< NondetTracking >( "nonteterminism-tracking", "" );
    }

    using lart::Pass::run;
    llvm::PreservedAnalyses run( llvm::Module &m ) override {
        _dl = std::make_unique< llvm::DataLayout >( &m );
        _ctx = &m.getContext();
        auto choice = m.getFunction( "__divine_choice" );
        auto ctype = choice->getFunctionType()->getParamType( 0 );

        std::vector< std::pair< llvm::CallInst *, Tracking > > toReplace;

        query::query( m ).flatten().flatten().map( query::refToPtr )
            .map( query::llvmdyncast< llvm::CallInst > )
            .filter( query::notnull )
            .map( [&]( llvm::CallInst *call ) {
                auto it = std::find_if( _choices.begin(), _choices.end(),
                        [call]( Choice &c ) { return c.match( call ); } );
                if ( it == _choices.end() )
                    return std::pair< Choice *, llvm::CallInst * >( nullptr, nullptr );
                return std::pair< Choice *, llvm::CallInst * >( &*it, call );
            } )
            .filter( []( auto t ) { return t.first != nullptr; } )
            .forall( [&]( auto t ) {
                    toReplace.emplace_back( t.second, this->trackAndReplace( t.first, t.second ) );
                } );

        for ( auto r : toReplace ) {
            if ( auto in = r.second.template asptr< Interval >() ) {
                llvm::IRBuilder<> irb( r.first );
                llvm::Value *replace;

                if ( in->size() > INT_MAX )
                    continue;

                replace = irb.CreateCall( choice, llvm::ConstantInt::get( ctype, in->size() ) );
                if ( replace->getType() != ctype ) {
                    if ( replace->getType()->getScalarSizeInBits() > ctype->getScalarSizeInBits() )
                        replace = irb.CreateTrunc( replace, ctype );
                    else
                        replace = irb.CreateZExtOrBitCast( replace, ctype );
                }
                if ( in->start != 0 ) {
                    replace = irb.CreateAdd( replace, llvm::ConstantInt::get( ctype, in->start ) );
                }

                r.first->replaceAllUsesWith( replace );
                r.first->eraseFromParent();
                replace->dump();
            }
        }

        return llvm::PreservedAnalyses::none();
    }

    struct Interval {
        int64_t start = 0;
        int64_t end = 0;

        Interval() = default;
        Interval( int64_t s, int64_t e ) : start( s ), end( e ) { }

        bool valid() const { return start != 0 || end != 0; }
        bool contains( int64_t x ) const { return start <= x && x <= end; }
        bool contains( Interval other ) const { return start <= other.start && other.end <= end; }
        uint64_t size() const { return uint64_t( end ) - uint64_t( start ) + 1; }

        bool operator==( Interval o ) const { return start == o.start && end == o.end; }
    };

    struct Choice {
        llvm::StringRef name;
        bool nameIsPrefix = false;
        llvm::Type *choiceType = nullptr;
        Interval interval;

        bool match( llvm::CallInst *call ) {
            if ( auto callee = getCalledFunction( call ) ) {
                return nameIsPrefix
                    ? callee->getName().startswith( name )
                    : callee->getName() == name;
            }
            return false;
        }

        llvm::Type *type( llvm::CallInst *call ) {
            if ( choiceType )
                return choiceType;
            if ( auto callee = call->getCalledFunction() )
                return callee->getFunctionType()->getReturnType();
            return nullptr;
        }
    };

    struct SIPair {
        Interval ifsigned;
        Interval ifunsigned;

        SIPair() = default;
        SIPair( Interval s, Interval u ) : ifsigned( s ), ifunsigned( u ) { }
    };

    using Tracking = brick::types::Union< Interval, SIPair >;

    std::string dump( Interval in ) {
        return "(" + std::to_string( in.start ) + ", " + std::to_string( in.end ) + ")";
    }

    std::string dump( Tracking t ) {
        if ( auto in = t.template asptr< Interval >() )
            return dump( *in );
        if ( auto si = t.template asptr< SIPair >() )
            return "[" + dump( si->ifsigned ) + "/" + dump( si->ifunsigned ) + "]";
        return "()";
    }

    Tracking trackAndReplace( Choice *choice, llvm::CallInst *call ) {
        Tracking in;
        if ( choice->interval.valid() )
            in = choice->interval;
        else if ( auto t = choice->type( call ) ) {
            if ( t->isIntegerTy( 1 ) )
                in = Interval( 0, 1 );
            else {
                auto intty = llvm::cast< llvm::IntegerType >( t );
                ASSERT_LEQ( intty->getBitWidth(), 64 );
                in = SIPair( { int64_t( intty->getSignBit() ),
                                int64_t( intty->getBitMask() ^ intty->getSignBit() ) },
                             { 0, int64_t( intty->getBitMask() ) } );
            }
        }
        else {
            std::cerr << "ERROR: could not track choice in " << call->getParent()->getParent()->getName().str() << std::endl;
            call->dump();
            std::abort();
        }

        LART_DEBUG( std::cerr << "INFO tracking init with " << dump( in ) << " for " << std::flush; call->dump() );
        return trackUsers( call, in );
    }

    Tracking track( llvm::Value *from, llvm::Value *v, Tracking tracking ) {
        LART_DEBUG( std::cerr << "    INFO: tracking " << dump( tracking ) << std::flush; v->dump() );

        bool matched = true;
        matched = llvmcase( v,
            [&]( llvm::TruncInst * ) { },
            []( llvm::ZExtInst * ) { /* ignored */ },
            []( llvm::SExtInst * ) { /* ignored */ },
            [&]( llvm::ICmpInst *cmp ) {
                if ( cmp->isEquality() ) {
                    ASSERT_EQ( cmp->getNumOperands(), 2 );
                    auto other = cmp->getOperand( 0 ) == from
                                ? cmp->getOperand( 1 )
                                : cmp->getOperand( 0 );
                    if ( auto consant = llvm::dyn_cast< llvm::ConstantInt >( other ) ) {
                        if ( consant->isZero() || consant->isOne() )
                            tracking = Interval( 0, 1 );
                            return;
                    }
                }
                matched = false;
                LART_DEBUG( cmp->dump() );
            },
            [&]( llvm::BinaryOperator *op ) {
                switch ( op->getOpcode() ) {
                    case llvm::Instruction::SRem:
                    case llvm::Instruction::URem:
                        ASSERT_EQ( op->getNumOperands(), 2 );
                        if ( auto mod = llvm::dyn_cast< llvm::ConstantInt >( op->getOperand( 1 ) ) ) {
                            int64_t modval = mod->getSExtValue();
                            Interval out;
                            if ( auto i = tracking.template asptr< Interval >() )
                                out = *i;
                            else if ( auto p = tracking.template asptr< SIPair >() ) {
                                if ( op->getOpcode() == llvm::Instruction::SRem )
                                    out = p->ifsigned;
                                else
                                    out = p->ifunsigned;
                            }

                            if ( out.start <= 0 && out.end >= modval ) {
                                out.start = 0;
                                out.end = modval;
                                tracking = out;
                                return;
                            }
                        }
                        // deliberately no break here
                    default:
                        matched = false;
                        LART_DEBUG( op->dump() );
                }
            } ) && matched;

        if ( !matched ) {
            LART_DEBUG( std::cerr << "    WARN: could not track " << std::flush; v->dump() );
            return tracking;
        }

        if ( auto in = tracking.template asptr< Interval >() )
            if ( in->size() < 16 ) {
                std::cerr << "    INFO: finished tracking with " << dump( *in  ) << std::endl;
                return tracking;
            }
        return trackUsers( v, tracking );
    }

    Tracking mkunion( Tracking x, Tracking y ) {
        if ( x.empty() )
            return y;
        if ( y.empty() )
            return x;
        NOT_IMPLEMENTED();
    }

    Tracking trackUsers( llvm::Value *v, const Tracking tracking ) {
        Tracking out;
        for ( auto u : v->users() ) {
            out = mkunion( track( v, u, tracking ), out );
        }
        return out;
    }

    NondetTracking() {
        Choice nondet;
        nondet.name = "__VERIFIER_nondet_";
        nondet.nameIsPrefix = true;
        _choices.emplace_back( nondet );
    }

  private:
    std::vector< Choice > _choices;
    std::unique_ptr< llvm::DataLayout > _dl;
    llvm::LLVMContext *_ctx;
};

struct Intrinsic : lart::Pass {

    static PassMeta meta() {
        return passMeta< Intrinsic >( "Intrinsic", "" );
    }

    void runFn( llvm::Function &fn ) {
        std::vector< llvm::CallInst * > asserts;
        std::vector< llvm::CallInst * > assumes;

        for ( auto call : query::query( fn ).flatten().map( query::refToPtr ).map( query::llvmdyncast< llvm::CallInst > ).filter( query::notnull ) ) {
            llvm::Value *calledVal = call->getCalledValue();

            llvm::Function *called = nullptr;
            do {
                llvmcase( calledVal,
                    [&]( llvm::Function *fn ) { called = fn; },
                    [&]( llvm::BitCastInst *bc ) { calledVal = bc->getOperand( 0 ); },
                    [&]( llvm::ConstantExpr *ce ) {
                        if ( ce->isCast() )
                            calledVal = ce->getOperand( 0 );
                        else {
                            LART_DEBUG( std::cerr << "CONST" << std::flush; ce->dump() );
                        }
                    },
                    [&]( llvm::Value *v ) { LART_DEBUG( v->dump() ); calledVal = nullptr; }
                );
            } while ( !called && calledVal );

            if ( !called )
                return;

            auto fname = called->getName();
            if ( fname == _verrorName )
                asserts.emplace_back( call );
            else if ( fname == _vassumeName )
                assumes.emplace_back( call );
        }

        for ( auto a : asserts )
            llvm::ReplaceInstWithInst( a, callProblem() );
        for ( auto a : assumes ) {
            ASSERT_EQ( a->getNumOperands(), 2 ); // 1 param + function to call
            llvm::Value *op = a->getArgOperand( 0 );
            auto vty = op->getType();
            if ( !vty->isIntegerTy( 1 ) ) {
                llvm::IRBuilder<> ins( a );
                if ( vty->isPointerTy() ) {
                    op = ins.CreatePtrToInt( op,  _intPtrT );
                    vty = _intPtrT;
                }
                ASSERT( vty->isIntegerTy() );
                op = ins.CreateICmpNE( op, llvm::ConstantInt::get( vty, 0 ) );
            }

            auto assumeFalse = llvm::BasicBlock::Create( *_ctx, "assume.false", &fn );
            auto origbb = a->getParent();
            llvm::Instruction *splitpt = std::next( llvm::BasicBlock::iterator( a ) );
            a->eraseFromParent();
            auto assumeTrue = origbb->splitBasicBlock( splitpt );
            auto br = origbb->getTerminator();
            llvm::ReplaceInstWithInst( br, llvm::BranchInst::Create( assumeTrue, assumeFalse, op ) );

            llvm::IRBuilder<> irb( assumeFalse );
            irb.CreateCall( _divineInterruptUnmask, { } );
            irb.CreateBr( assumeFalse );
//            irb.CreateCall( _exit, { llvm::ConstantInt::get( llvm::Type::getInt32Ty( *_ctx ), 0 ) } );
//            irb.CreateUnreachable();
        }
    }

    using lart::Pass::run;
    llvm::PreservedAnalyses run( llvm::Module &m ) override {
        _ctx = &m.getContext();
        _divineProblem = m.getFunction( "__divine_problem" );
        _divineInterruptUnmask = m.getFunction( "__divine_interrupt_unmask" );
        _exit = m.getFunction( "_exit" );
        llvm::DataLayout dl( &m );
        _intPtrT = llvm::Type::getIntNTy( *_ctx, dl.getPointerSizeInBits() );

        for ( auto &fn : m )
            runFn( fn );

        auto dropfn = [&]( llvm::StringRef name ) {
            if ( auto fn = m.getFunction( name ) ) {
                for ( auto u : fn->users() ) {
                    LART_DEBUG( std::cerr << "U " << std::flush; u->dump() );
                    static_cast< void >( u );
                }
                fn->replaceAllUsesWith( llvm::UndefValue::get( fn->getType() ) );
                fn->eraseFromParent();
            }
        };
        dropfn( _vassumeName );
        dropfn( _verrorName );

        return llvm::PreservedAnalyses::none();
    }

    llvm::CallInst *callProblem() {
        return llvm::CallInst::Create( _divineProblem, llvm::ArrayRef< llvm::Value * >( {
                    static_cast< llvm::Value * >( llvm::ConstantInt::get( llvm::Type::getInt32Ty( *_ctx ), 2 ) ),
                    static_cast< llvm::Value * >( llvm::ConstantPointerNull::get( llvm::Type::getInt8PtrTy( *_ctx ) ) )
                } ) );
    }

  private:
      llvm::LLVMContext *_ctx;
      llvm::Function *_divineProblem;
      llvm::Function *_divineInterruptUnmask;
      llvm::Function *_exit;
      llvm::Type *_intPtrT;
      static llvm::StringRef _verrorName;
      static llvm::StringRef _vassumeName;
};

llvm::StringRef Intrinsic::_verrorName = "__VERIFIER_error";
llvm::StringRef Intrinsic::_vassumeName = "__VERIFIER_assume";

struct NoMallocFail : lart::Pass {

    static PassMeta meta() {
        return passMeta< NoMallocFail >( "NoMallocFail", "" );
    }

    void dechoice( llvm::Function *fn ) {
        if ( !fn || fn->empty() )
            return;

        int removed = 0;

        auto toRemove = query::query( *fn ).flatten()
            .map( query::llvmdyncast< llvm::CallInst > )
            .filter( query::notnull )
            .filter( [&]( llvm::CallInst *call ) { return call->getCalledFunction() == choice; } )
            .freeze();

        for ( llvm::CallInst *call : toRemove ) {
            call->replaceAllUsesWith( llvm::ConstantInt::get( llvm::Type::getInt32Ty( *ctx ), 1 ) );
            call->eraseFromParent();
            ++removed;
        }

        std::cout << "INFO: removed " << removed << " choices from " << fn->getName().str() << std::endl;
    }

    using lart::Pass::run;
    llvm::PreservedAnalyses run( llvm::Module &m ) override {
        choice = m.getFunction( "__divine_choice" );
        ctx = &m.getContext();

        dechoice( m.getFunction( "malloc" ) );
        dechoice( m.getFunction( "realloc" ) );
        dechoice( m.getFunction( "calloc" ) );
        return llvm::PreservedAnalyses::none();
    }

    llvm::Function *choice = nullptr;
    llvm::LLVMContext *ctx = nullptr;
};

PassMeta intrinsicPass() {
    return compositePassMeta< NondetTracking, NoMallocFail, Intrinsic >( "svcomp", "" );
}

#undef LART_DEBUG

}
}
