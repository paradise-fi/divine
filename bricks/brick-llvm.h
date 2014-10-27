// -*- C++ -*- (c) 2014 Vladimír Štill

#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS
#endif
#ifndef __STDC_CONSTANT_MACROS
#define __STDC_CONSTANT_MACROS
#endif
#include <llvm/Linker.h>
#include <llvm/Bitcode/ReaderWriter.h>
#include <llvm/IR/GlobalValue.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/Analysis/Verifier.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/raw_ostream.h>

#include <brick-unittest.h>

#include <vector>
#include <string>
#include <initializer_list>

#ifndef BRICKS_LLVM_LINK_STRIP_H
#define BRICKS_LLVM_LINK_STRIP_H

namespace brick {
namespace llvm {

struct Linker {
    Linker( ::llvm::Module *root ) :  _link( root ) {
        ASSERT( root != nullptr );
    }

    bool link( ::llvm::Module *src, std::string *emsg = nullptr ) {
        ASSERT( src != nullptr );
        return _link.linkInModule( src, ::llvm::Linker::DestroySource, emsg );
    }

    template< typename Roots = std::initializer_list< std::string > >
    ::llvm::Module *prune( Roots roots ) {
        return Prune( this, std::move( roots ) ).prune();
    }

    ::llvm::Module *get() {
        return _link.getModule();
    }

  private:
    ::llvm::Linker _link;

    struct Prune {
        Linker *_link;
        std::vector< ::llvm::Value * > _stack;
        std::set< ::llvm::Value * > _seen;
        long _functions = 0, _globals = 0;

        template< typename Roots >
        Prune( Linker *ln, Roots roots ) : _link( ln ) {
            for ( auto sym : roots )
                if ( auto root = ln->get()->getFunction( sym ) )
                    push( root );
            for ( auto sym : roots )
                if ( auto root = ln->get()->getGlobalVariable( sym, true ) )
                    push( root );
        }

        ::llvm::Module *prune() {
            while ( _stack.size() ) {
                auto val = _stack.back();
                _stack.pop_back();
                ASSERT( val != nullptr );

                handleVal( val );
            }

            auto m = _link->get();

            std::vector< ::llvm::GlobalValue* > toDrop;

            for ( auto &fn : *m )
                if ( !_seen.count( &fn ) )
                    toDrop.push_back( &fn );

            for ( auto glo = m->global_begin(); glo != m->global_end(); ++glo )
                if ( !_seen.count( &*glo ) )
                    toDrop.push_back( &*glo );

            for ( auto ali = m->alias_begin(); ali != m->alias_end(); ++ali )
                if ( !_seen.count( &*ali ) )
                    toDrop.push_back( &*ali );

            /* we must first delete bodies of functions (and globals),
             * then drop referrences to them (replace with undef)
             * finally we can erase symbols
             * Those must be separate runs, otherwise we could trip SEGV
             * by accessing already freed symbol by some use
             */
            for ( auto glo : toDrop )
                glo->dropAllReferences();

            for ( auto glo : toDrop )
                glo->replaceAllUsesWith( ::llvm::UndefValue::get( glo->getType() ) );

            for ( auto glo : toDrop ) {
                glo->eraseFromParent();
            }

            return m;
        }

        void push( ::llvm::Value *val ) {
            if ( _seen.insert( val ).second )
                _stack.push_back( val );
        }

        void handleVal( ::llvm::Value *val ) {
            ASSERT( val != nullptr );

            if ( auto fn = ::llvm::dyn_cast< ::llvm::Function >( val ) ) {
                ++_functions;

                for ( const auto &bb : *fn )
                    for ( const auto &ins : bb )
                        for ( auto op = ins.op_begin(); op != ins.op_end(); ++op )
                            push( op->get() );
            }

            else if ( auto bb = ::llvm::dyn_cast< ::llvm::BasicBlock >( val ) )
                push( bb->getParent() );

            else if ( auto ba = ::llvm::dyn_cast< ::llvm::BlockAddress >( val ) )
                push( ba->getFunction() );

            else if ( auto ali = ::llvm::dyn_cast< ::llvm::GlobalAlias >( val ) ) {
                push( ali->getAliasee() );
            }

            else if ( auto global = ::llvm::dyn_cast< ::llvm::GlobalValue >( val ) ) {
                ++_globals;

                if ( auto gvar = ::llvm::dyn_cast< ::llvm::GlobalVariable >( global ) )
                    if ( gvar->hasInitializer() )
                        push( gvar->getInitializer() );
            }

            // bitcast function pointers
            else if ( auto ce = ::llvm::dyn_cast< ::llvm::ConstantExpr >( val ) )
                push( ce->getAsInstruction() );

            // catchall: instructions, constants, ...
            else if ( auto ins = ::llvm::dyn_cast< ::llvm::User >( val ) )
                for ( auto op = ins->op_begin(); op != ins->op_end(); ++op )
                    push( op->get() );
        }
    };
};

inline void writeModule( ::llvm::Module *m, std::string out ) {
    std::string serr;
    ::llvm::raw_fd_ostream fs( out.c_str(), serr );
    WriteBitcodeToFile( m, fs );
}

}
}

#endif // BRICKS_LLVM_LINK_STRIP_H
