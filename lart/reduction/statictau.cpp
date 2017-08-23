// -*- C++ -*- (c) 2015 Vladimír Štill <xstill@fi.muni.cz>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/CallSite.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/reduction/passes.h>
#include <lart/support/pass.h>
#include <lart/support/util.h>
#include <lart/support/query.h>

#include <brick-hlist>

namespace lart {
namespace reduction {

unsigned getSilentKind( llvm::Module &m ) {
    return m.getMDKindID( "lart.silent" );
}

struct SimpleEscape {

    static PassMeta meta() {
        return passMeta< SimpleEscape >( "SimpleEscape",
                "simple escape analysis and __vm_interrupt_mem removeal for memory accesses within a function" );
    }

    static bool isAllocation( llvm::Instruction *i ) {
        if ( llvm::isa< llvm::AllocaInst >( i ) )
            return true;
        llvm::CallSite cs( i );
        if ( !cs || !cs.getCalledFunction() )
            return false;
        auto name = cs.getCalledFunction()->getName();
        return name == "__vm_obj_make"
               || name == "malloc" || name == "calloc" || name == "realloc"
               || name.startswith( "_Znwm" ) || name.startswith( "_Znam" );
    }

    struct EscapeVisitor {

        bool _visit( llvm::Instruction *, llvm::Instruction * ) {
            return false;
        }
        bool _visit( llvm::DbgInfoIntrinsic *, llvm::Instruction * ) { return true; }
        // ret is OK, if there are not other escapes the value cannot escape in this function
        bool _visit( llvm::ReturnInst *, llvm::Instruction * ) { return true; }
        bool _visit( llvm::LoadInst *l, llvm::Instruction * ) {
            return collect( l );
        }
        bool _visit( llvm::StoreInst *i, llvm::Instruction *alloc ) {
            return i->getValueOperand()->stripPointerCasts() != alloc
                && collect( i );
        }
        bool _visit( llvm::AtomicRMWInst *i, llvm::Instruction *alloc ) {
            return i->getValOperand()->stripPointerCasts() != alloc
                && collect( i );
        }
        bool _visit( llvm::AtomicCmpXchgInst *i, llvm::Instruction *alloc ) {
            return i->getCompareOperand()->stripPointerCasts() != alloc
                && i->getNewValOperand()->stripPointerCasts() != alloc
                && collect( i );
        }
        bool _visit( llvm::BitCastInst *i, llvm::Instruction *alloc ) {
            return i->stripPointerCasts() == alloc && run( i, alloc );
        }

        bool collect( llvm::Instruction *i ) {
            collected.push_back( i );
            return true;
        }

        bool run( llvm::Instruction *i, llvm::Instruction *alloc = nullptr ) {
            alloc = alloc ?: i;
            return query::query( i->users() )
                    .map( query::llvmdyncast< llvm::Instruction > )
                    .filter( query::notnull )
                    .all( [=]( llvm::Instruction *i ) {
                        return applyInst( i, [=]( auto x ) { return _visit( x, alloc ); } );
                    } );
        }

        std::vector< llvm::Instruction * > collected;
    };

    void run( llvm::Module &m ) {
        long all = 0, silenced = 0;
        auto allocations = query::query( m ).flatten().flatten().map( query::refToPtr )
                                            .filter( isAllocation );
        auto &ctx = m.getContext();
        auto *mdstr = llvm::MDString::get( ctx, silentTag );
        auto *meta = llvm::MDNode::get( ctx, { mdstr } );
        auto mid = m.getMDKindID( silentTag );

        for ( auto a : allocations ) {
            ++all;
            EscapeVisitor ev;
            if ( ev.run( a ) ) {
                ++silenced;
                for ( auto *i : ev.collected ) {
                    i->setMetadata( mid, meta );
                }
                a->setMetadata( mid, meta );
            }
        }
    }

  private:
    bool canBeCaptured( llvm::Value *v ) { return util::canBeCaptured( v, &_escMap ); }

    util::Map< llvm::Value *, bool > _escMap;
};

PassMeta staticTauMemPass() {
    return compositePassMeta< SimpleEscape >( "statictaumem", "mark memory instructions which are known to not have effect obeservable by other threads as silent" );
}


}
}
