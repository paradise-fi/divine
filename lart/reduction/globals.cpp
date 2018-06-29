// -*- C++ -*- (c) 2015 Vladimír Štill <xstill@fi.muni.cz>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/CallSite.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/reduction/passes.h>
#include <lart/support/pass.h>
#include <lart/support/util.h>
#include <lart/support/query.h>

#include <brick-hlist>

namespace lart {
namespace reduction {

struct ReadOnlyGlobals {

    static PassMeta meta() {
        return passMeta< ReadOnlyGlobals >( "ReadOnlyGlobals" );
    }

    bool readOnly( llvm::GlobalValue &glo ) { return dispatchReadOnly( &glo ); }

    bool dispatchReadOnly( llvm::Value *v ) {
        auto it = _romap.find( v );
        if ( it != _romap.end() )
            return it->second;
        bool r = query::query( v->users() )
                  .all( [=]( auto *inst ) { return this->dispatchIsLoad( v, inst ); } );
        return _romap[ v ] = r;
    }

    bool dispatchIsLoad( llvm::Value *v, llvm::Value *inst ) {
        std::pair< llvm::Value *, llvm::Value * > pair{ v, inst };
        auto it = _loadmap.find( pair );
        if ( it != _loadmap.end() )
            return it->second;
        bool r = apply( inst, brick::hlist::TypeList<
                    llvm::LoadInst, llvm::AllocaInst, // these are inherently safe
                    llvm::CallInst, llvm::InvokeInst, // need to track params
                    // for all the other it is sufficient to check that result
                    // does not participate in any unsafe operation (store,…),
                    // so just recurse on result (if there is any used)
                    llvm::GetElementPtrInst, llvm::ExtractValueInst, llvm::ExtractElementInst,
                    llvm::InsertValueInst, llvm::InsertElementInst,
                    llvm::ConstantExpr,
                    llvm::BinaryOperator, llvm::CmpInst,
                    llvm::SelectInst, llvm::ShuffleVectorInst,
                    llvm::BranchInst, llvm::IndirectBrInst, llvm::SwitchInst,
                    llvm::CastInst, llvm::PHINode
                    >(),
                false, [=]( auto *inst ) { return this->detectIsLoad( v, inst ); } );
        return _loadmap[ pair ] = r;
    }

    bool detectIsLoad( llvm::Value *, llvm::LoadInst * ) { return true; }
    bool detectIsLoad( llvm::Value *, llvm::AllocaInst * ) { return true; }

    bool detectIsLoad( llvm::Value *v, llvm::CallInst *c ) {
        return detectIsLoad( v, llvm::CallSite( c ) );
    }
    bool detectIsLoad( llvm::Value *v, llvm::InvokeInst *i ) {
        return detectIsLoad( v, llvm::CallSite( i ) );
    }
    bool detectIsLoad( llvm::Value *v, llvm::CallSite cs ) {
        auto *fn = llvm::dyn_cast< llvm::Function >( cs.getCalledValue()->stripPointerCasts() );
        if ( !fn ) // indirect call, need pointer analysis :-/
            return false;
        auto ait = fn->arg_begin(), aend = fn->arg_end();

        bool all = true;
        // must iterate through all actual parameters, if no formal param is there
        // for any of them must return false
        for ( int i = 0, e = cs.arg_size(); all && i < e; ++i, ++ait )
            if ( cs.getArgument( i ) == v )
                all = ait != aend && dispatchReadOnly( &*ait );
        return all;
    }

    template< typename T >
    bool detectIsLoad( llvm::Value *, T *x ) {
        ASSERT( !llvm::isa< llvm::LoadInst >( x ) );
        ASSERT( !llvm::isa< llvm::CallInst >( x ) );
        ASSERT( !llvm::isa< llvm::InvokeInst >( x ) );
        return dispatchReadOnly( x );
    }

    void run( llvm::Module &m ) {
        long all = 0, constified = 0;
        for ( auto &glo : m.globals() ) {
            if ( !glo.isConstant() && !glo.isExternallyInitialized() && glo.hasUniqueInitializer() ) {
                ++all;
                if ( readOnly( glo ) ) {
                    ++constified;
                    glo.setConstant( true );
                }
            }
        }

        std::cerr << "INFO: constified " << constified << " global out of " << all << " candidates" << std::endl;

    }

  private:
    util::Map< llvm::Value *, bool > _romap;
    util::Map< std::pair< llvm::Value *, llvm::Value * >, bool > _loadmap;
};

PassMeta globalsPass() {
    return compositePassMeta< ReadOnlyGlobals >( "globals", "Optimize usage of global variables" );
}


}
}
