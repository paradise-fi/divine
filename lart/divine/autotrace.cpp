// -*- C++ -*- (c) 2016 Vladimír Štill <xstill@fi.muni.cz>

DIVINE_RELAX_WARNINGS
#include <llvm/Pass.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IRBuilder.h>
DIVINE_UNRELAX_WARNINGS

#include <brick-string>
#include <string>
#include <iostream>
#include <cxxabi.h>

#include <lart/support/pass.h>
#include <lart/support/meta.h>
#include <lart/support/util.h>
#include <lart/support/metadata.h>
#include <lart/support/cleanup.h>

namespace lart {
namespace divine {

struct Autotrace : lart::Pass {

    using Vals = std::vector< llvm::Value * >;

    static PassMeta meta() {
        return passMeta< Autotrace >( "Autotrace", "" );
    }

    using lart::Pass::run;
    llvm::PreservedAnalyses run( llvm::Module &m ) override {
        // void __dios_trace( int upDown, const char *p, ... )
        auto *trace = m.getFunction( "__dios_trace_auto" );
        if ( !trace )
            return llvm::PreservedAnalyses::all();

        if ( !tagModuleWithMetadata( m, "lart.divine.autotrace" ) )
            return llvm::PreservedAnalyses::all();

        auto *traceT = trace->getFunctionType();
        traceUp = llvm::ConstantInt::get( traceT->getParamType( 0 ), 1 );
        traceDown = llvm::ConstantInt::get( traceT->getFunctionParamType( 0 ), -1 );

        auto ehInfo = cleanup::EhInfo::cpp( m );

        for ( auto &fn : m ) {
            if ( fn.empty() || &fn == trace )
                continue;

            llvm::IRBuilder<> irb( fn.front().getFirstInsertionPt() );
            irb.CreateCall( trace, callArgs( fn, irb ) );
            ++entry;

            cleanup::makeExceptionsVisible( ehInfo, fn, []( const auto & ) { return true; } );
            cleanup::atExits( fn, [&]( llvm::Instruction *i ) {
                    llvm::IRBuilder<> irb( i );
                    irb.CreateCall( trace, applyInst( i, [&]( auto *i ) {
                                              return retArgs( i, irb ); } ) );
                    ++exit;
                } );
        }
        return llvm::PreservedAnalyses::none();
    }

    std::string demangle( std::string x ) {
        int stat;
        char *dem = abi::__cxa_demangle( x.c_str(), nullptr, nullptr, &stat );
        if ( dem ) {
            std::string strdem( dem );
            std::free( dem );
            return strdem;
        }
        return x;
    }

    std::pair< std::string, llvm::Value * > fmtVal( llvm::Value &arg, llvm::IRBuilder<> &irb ) {
        std::string lit;
        llvm::Value *val = nullptr;

        auto t = arg.getType();

        if ( t->isPointerTy() )
        {
            lit = "%p";
            val = &arg;
        }
        else if ( auto *it = llvm::dyn_cast< llvm::IntegerType >( t ) )
        {
            if ( it->getBitWidth() < 64 )
                val = irb.CreateSExt( &arg, irb.getInt64Ty() );
            else if ( it->getBitWidth() == 64 )
                val = &arg;
            else {
                lit = "(truncated) ";
                val = irb.CreateTrunc( &arg, irb.getInt64Ty() );
            }
            lit += "%lld";
        }
        else if ( t->isFloatTy() )
        {
            lit += "%f";
            val = irb.CreateFPCast( &arg, irb.getDoubleTy() );
        }
        else if ( t->isDoubleTy() )
        {
            lit = "%f, ";
            val = &arg;
        }
        else
            lit = "(unknown struct or weird type), ";
        return { lit, val };
    }

    Vals callArgs( llvm::Function &fn, llvm::IRBuilder<> &irb ) {
        auto name = demangle( fn.getName().str() );
        std::string fmt = "call " + name;
        Vals vals;
        vals.push_back( traceUp );
        vals.push_back( 0 );

        if ( fn.arg_begin() != fn.arg_end() )
            fmt += ": ";

        for ( auto &arg : fn.args() ) {
            auto f = fmtVal( arg, irb );
            fmt += f.first + ", ";
            if ( f.second )
                vals.push_back( f.second );
        }
        if ( fmt.back() == ' ' )
            fmt.pop_back();
        if ( fmt.back() == ',' )
            fmt.pop_back();

        vals[ 1 ] = getLit( fmt, irb );

        return vals;
    }

    llvm::Value *getLit( std::string lit, llvm::IRBuilder<> irb ) {
        lit.push_back( 0 );
        auto it = litmap.find( lit );
        if ( it != litmap.end() )
            return it->second;
        return litmap.emplace( lit, irb.CreatePointerCast( util::getStringGlobal( lit,
                                      *irb.GetInsertBlock()->getParent()->getParent() ),
                                  irb.getInt8PtrTy() ) ).first->second;
    }

    Vals retArgs( llvm::ReturnInst *i, llvm::IRBuilder<> &irb ) {
        if ( !i->getReturnValue() )
            return { traceDown, getLit( "return void", irb ) };

        auto f = fmtVal( *i->getReturnValue(), irb );
        Vals vals { traceDown, getLit( "return " + f.first, irb ) };
        if ( f.second )
            vals.push_back( f.second );
        return vals;
    }

    Vals retArgs( llvm::ResumeInst *, llvm::IRBuilder<> &irb ) {
        return { traceDown, getLit( "exiting function with active exception", irb ) };
    }

    Vals retArgs( llvm::Instruction *, llvm::IRBuilder<> &irb )
    {
        return { traceDown, llvm::ConstantPointerNull::get( irb.getInt8PtrTy() ) };
    }

    long entry = 0, exit = 0;
    llvm::Constant *traceUp = nullptr, *traceDown = nullptr;
    util::Map< std::string, llvm::Value * > litmap;
};

PassMeta autotracePass() {
    return compositePassMeta< Autotrace >( "autotrace",
            "Instrument bitcode for DIVINE: add trace calls to function begins and ends." );
}

}
}

