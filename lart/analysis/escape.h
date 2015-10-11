// -*- C++ -*- (c) 2015 Vladimír Štill <xstill@fi.muni.cz>

#include <llvm/IR/PassManager.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/CallSite.h>

#include <lart/support/util.h>

#include <unordered_map>

#ifndef LART_ESCAPE_ANALYSIS_H
#define LART_ESCAPE_ANALYSIS_H

namespace lart {
namespace analysis {

/*

TODO: %2 also escapes:

%1 = alloca
store %global, %1
%2 = bitcast %1

but this is not currently handled!
*/
struct EscapeAnalysis {

    EscapeAnalysis( llvm::Module &module ) :
        _module( module ), _layout( &module ), _ptrBits( _layout.getPointerSizeInBits() )
    { }

    struct Analysis {

        bool dirty() const { return _dirty; }
        void startIteration() { _dirty = false; ++_iteration; }
        int iteration() const { return _iteration; }

        void addEscape( llvm::Value *what, llvm::Instruction *where ) {
            if ( _escapes[ what ].insert( where ).second ) {
                _dirty = true;
            }
        }
        void addUse( llvm::Value *what, llvm::Instruction *where ) {
            auto it = _escapes.find( where );
            if ( it != _escapes.end() )
                for ( auto i : it->second )
                    if ( _escapes[ what ].insert( i ).second ) {
                        _dirty = true;
                    }
        }

        const std::unordered_set< llvm::Instruction * > &escapes( llvm::Instruction *what ) {
            return _escapes[ what ];
        }

      private:
        bool _dirty = true;
        int _iteration = 0;
        // mapping instruction -> where it escapes
        std::unordered_map< llvm::Value *, std::unordered_set< llvm::Instruction * > > _escapes;
    };
    using Result = Analysis;

    void dropAnalysis( llvm::Function *fn ) { _cache.erase( fn ); }

    Analysis run( llvm::Function &fn ) { return analyze( fn ); }
    Analysis analyze( llvm::Function &fn ) { return analyze( &fn ); }
    Analysis analyze( llvm::Function *fn ) {
        Analysis an;

        auto addEscape = [&]( llvm::Value *val, llvm::Instruction *inst ) {
            if ( canBePointer( val->getType() ) )
                an.addEscape( val, inst );
        };

        while ( an.dirty() ) {
            an.startIteration();
            bbPostorder( *fn, [&]( llvm::BasicBlock *bb ) {
                for ( auto &inst : util::reverse( *bb ) ) {
                    llvmcase( inst,
                        []( llvm::DbgDeclareInst * ) { }, // ignore
                        [&]( llvm::StoreInst *store ) {
                            addEscape( store->getValueOperand(), store );
                        },
                        [&]( llvm::AtomicCmpXchgInst *cas ) {
                            addEscape( cas->getNewValOperand(), cas );
                        },
                        [&]( llvm::AtomicRMWInst *rmw ) {
                            addEscape( rmw->getValOperand(), rmw );
                        },
                        [&]( llvm::CallInst *call ) {
                            for ( auto &arg : call->arg_operands() )
                                addEscape( arg, call );
                        },
                        [&]( llvm::InvokeInst *invoke ) {
                            for ( auto &arg : invoke->arg_operands() )
                                addEscape( arg, invoke );
                        },
                        [&]( llvm::Instruction *inst ) {
                            for ( auto &op : inst->operands() )
                                an.addUse( op, inst );
                        } );
                }
            } );
        }
//        std::cerr << "INFO: analyzed " << fn->getName().str() << " in " << an.iteration() << " iterations" << std::endl;
        return an;
    }

    Analysis &operator[]( llvm::Function *fn ) {
        auto it = _cache.find( fn );
        if ( it == _cache.end() )
            return _cache[ fn ] = analyze( fn );
        return it->second;
    }

  private:
    llvm::Module &_module;
    llvm::DataLayout _layout;
    std::unordered_map< llvm::Function *, Analysis > _cache;
    unsigned _ptrBits;

    bool canBePointer( llvm::Type *t ) {
        return t->isPointerTy() || t->getPrimitiveSizeInBits() > _ptrBits;
    }

};

} // namespace analysis
} // namespace lart

#endif // LART_ESCAPE_ANALYSIS_H
