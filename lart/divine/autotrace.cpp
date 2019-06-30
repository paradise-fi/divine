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

struct Autotrace {

    using Vals = std::vector< llvm::Value * >;

    bool _calls = false, _returns = false, _allocs = false;
    std::set< llvm::Function * > _skip;
    llvm::Function *_fn_trace, *_fn_mkobj, *_fn_frobj;

    Autotrace( std::string opt )
    {
        if ( opt.find( "allocs" ) != std::string::npos )
            _allocs = true;
        if ( opt.find( "calls" ) != std::string::npos )
            _calls = true;
        if ( opt.find( "returns" ) != std::string::npos )
            _returns = true;
        if ( !_calls && !_allocs )
            throw std::logic_error( "constructed a useless Autotrace instance" );
    }

    void handle_calls( llvm::Function &fn )
    {
        if ( _skip.count( &fn ) )
            return;

        if ( _calls )
        {
            llvm::IRBuilder<> irb( &*fn.front().getFirstInsertionPt() );
            irb.CreateCall( _fn_trace, callArgs( fn, irb ) );
            ++entry;
        }

        auto ehInfo = cleanup::EhInfo::cpp( *fn.getParent() );

        cleanup::makeExceptionsVisible( ehInfo, fn, []( const auto & ) { return true; } );
        cleanup::atExits( fn, [&]( llvm::Instruction *i )
        {
            llvm::IRBuilder<> irb( i );
            irb.CreateCall( _fn_trace, applyInst( i, [&]( auto *i ) { return retArgs( i, irb ); } ) );
            ++exit;
        } );
    }

    void handle_alloc( llvm::Function &fn, llvm::BasicBlock::iterator inst )
    {
        auto call = llvm::dyn_cast< llvm::CallInst >( inst );
        if ( !call ) return;
        llvm::IRBuilder<> irb( &*std::next( inst ) );
        llvm::Value *fmt = nullptr, *operand = call;
        if ( call->getCalledFunction() == _fn_mkobj )
            fmt = getLit( "allocated %p in " + fn.getName().str(), irb );
        if ( call->getCalledFunction() == _fn_frobj )
        {
            fmt = getLit( "deleted %p in " + fn.getName().str(), irb );
            operand = call->getOperand( 0 );
        }
        if ( fmt )
            irb.CreateCall( _fn_trace, { traceStay, fmt, operand } );
    }

    void handle_allocs( llvm::Function &fn )
    {
        for ( auto &bb : fn )
            for ( auto inst = bb.begin(); inst != bb.end(); ++inst )
                handle_alloc( fn, inst );
    }

    void run( llvm::Module &m )
    {
        // void __dios_trace( int upDown, const char *p, ... )
        _fn_mkobj = m.getFunction( "__vm_obj_make" );
        _fn_frobj = m.getFunction( "__vm_obj_free" );
        _fn_trace = m.getFunction( "__dios_trace_auto" );
        if ( !_fn_trace )
            return;

        brick::llvm::enumerateFunctionsForAnno( "lart.boring", m,
                                                [&]( llvm::Function *f ) { _skip.insert( f ); } );

        auto *suspend = m.getFunction( "__dios_suspend" );
        auto *resched = m.getFunction( "__dios_reschedule" );

        if ( !tagModuleWithMetadata( m, "lart.divine.autotrace" ) )
            return;

        auto *traceT = _fn_trace->getFunctionType();
        traceUp   = llvm::ConstantInt::get( traceT->getParamType( 0 ),  1 );
        traceStay = llvm::ConstantInt::get( traceT->getParamType( 0 ),  0 );
        traceDown = llvm::ConstantInt::get( traceT->getParamType( 0 ), -1 );

        for ( auto &fn : m )
        {
            if ( fn.empty() || &fn == _fn_trace || &fn == suspend || &fn == resched )
                continue;

            if ( _calls )
                handle_calls( fn );
            if ( _allocs )
                handle_allocs( fn );
        }
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

    Vals retArgs( llvm::ReturnInst *i, llvm::IRBuilder<> &irb )
    {
        if ( !_returns )
            return { traceDown, getLit( "", irb ) };

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
    llvm::Constant *traceUp = nullptr, *traceDown = nullptr, *traceStay = nullptr;
    util::Map< std::string, llvm::Value * > litmap;
};

PassMeta autotracePass()
{
    return passMetaO< Autotrace >( "autotrace", "Insert tracing calls at points of interest." );
}

}
}

