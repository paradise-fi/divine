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
        auto *trace = m.getFunction( "__dios_trace" );
        if ( !trace )
            return llvm::PreservedAnalyses::all();

        if ( !tagModuleWithMetadata( m, "lart.divine.autotrace" ) )
            return llvm::PreservedAnalyses::all();

        auto *traceT = trace->getFunctionType();
        traceUp = llvm::ConstantInt::get( traceT->getParamType( 0 ), 1 );
        traceDown = llvm::ConstantInt::get( traceT->getFunctionParamType( 0 ), -1 );

        auto ehInfo = cleanup::EhInfo::cpp( m );

        for ( auto &fn : m ) {
            if ( fn.empty() )
                continue;

            llvm::IRBuilder<> irb( fn.front().getFirstInsertionPt() );
            irb.CreateCall( trace, callArgs( fn, irb ) );
            ++entry;

            cleanup::makeExceptionsVisible( ehInfo, fn, []( const auto & ) { return true; } );
            cleanup::atExits( fn, [&]( llvm::Instruction *i ) {
                    llvm::IRBuilder<> irb( i );
                    irb.CreateCall( trace, retArgs( fn, i, irb ) );
                    ++exit;
                } );
        }
        std::cout << "Annotated " << entry << " and " << exit << " exit points" << std::endl;
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

    Vals callArgs( llvm::Function &fn, llvm::IRBuilder<> &irb ) {
        auto name = demangle( fn.getName().str() );
        std::string fmt = "Call to " + name;
        auto *typ = fn.getFunctionType();
        Vals vals;
        vals.push_back( traceUp );
        vals.push_back( 0 );

        if ( fn.arg_begin() != fn.arg_end() )
            fmt += " with ";

        for ( auto &arg : fn.args() ) {
            auto t = arg.getType();

            if ( t->isPointerTy() )
            {
                fmt += "%p, ";
                vals.push_back( &arg );
            }
            else if ( auto *it = llvm::dyn_cast< llvm::IntegerType >( t ) )
            {
                if ( it->getBitWidth() < 64 )
                    vals.push_back( irb.CreateSExt( &arg, irb.getInt64Ty() ) );
                else if ( it->getBitWidth() == 64 )
                    vals.push_back( &arg );
                else {
                    fmt += "(truncated) ";
                    vals.push_back( irb.CreateTrunc( &arg, irb.getInt64Ty() ) );
                }
                fmt += "%lld, ";
            }
            else if ( t->isFloatTy() )
            {
                fmt += "%f, ";
                vals.push_back( irb.CreateFPCast( &arg, irb.getDoubleTy() ) );
            }
            else if ( t->isDoubleTy() )
            {
                fmt += "%f, ";
                vals.push_back( &arg );
            }
            else
                fmt += "(unknown struct or weird type), ";
        }
        if ( fmt.back() == ' ' )
            fmt.pop_back();
        if ( fmt.back() == ',' )
            fmt.pop_back();
        fmt.push_back( 0 ); // null-terminate cstring

        vals[ 1 ] = irb.CreatePointerCast( util::getStringGlobal( fmt, *fn.getParent() ),
                                           irb.getInt8PtrTy() );

        return vals;
    }

    Vals retArgs( llvm::Function &fn, llvm::Instruction *i, llvm::IRBuilder<> &irb )
    {
        return { traceDown, llvm::ConstantPointerNull::get( irb.getInt8PtrTy() ) };
    }

    long entry = 0, exit = 0;
    llvm::Constant *traceUp = nullptr, *traceDown = nullptr;
};

PassMeta autotracePass() {
    return compositePassMeta< Autotrace >( "autotrace",
            "Instrument bitcode for DIVINE: add trace calls to function begins and ends." );
}

}
}

