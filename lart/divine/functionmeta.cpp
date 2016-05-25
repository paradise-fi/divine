// -*- C++ -*- (c) 2016 Vladimír Štill <xstill@fi.muni.cz>

DIVINE_RELAX_WARNINGS
#include <llvm/Pass.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
DIVINE_UNRELAX_WARNINGS

#include <brick-string>
#include <string>
#include <iostream>

#include <lart/support/pass.h>
#include <lart/support/meta.h>
#include <lart/support/query.h>
#include <lart/support/util.h>
#include <lart/support/metadata.h>

namespace lart {
namespace divine {

struct FunctionMeta {
    template< typename Index >
    explicit FunctionMeta( llvm::Function &fn, Index &index ) :
        entryPoint( &fn ),
        frameSize( 0 ), // filed by loader
        argCount( fn.arg_size() ),
        isVariadic( fn.isVarArg() ),
        insts( query::query( fn ).flatten().map( [&]( llvm::Instruction &i ) {
                // Instructio info contains opcode and suboperation (for ARMW and *CMP)
                return llvm::ConstantStruct::get( index._instMetaT, {
                        index.mkint( index._instMetaT, 0, i.getOpcode() ),
                        index.mkint( index._instMetaT, 1, getOperation( i ) )
                    } );
            } ).freeze() )
    { }

    static int getOperation( llvm::Instruction &i ) {
        if ( auto *armw = llvm::dyn_cast< llvm::AtomicRMWInst >( &i ) )
            return armw->getOperation();
        if ( auto *cmp = llvm::dyn_cast< llvm::CmpInst >( &i ) )
            return cmp->getPredicate();
        return 0;
    }

    llvm::Function *entryPoint;
    int frameSize;
    int argCount;
    bool isVariadic;
    std::vector< llvm::Constant * > insts;
};

struct IndexFunctions : lart::Pass {

    static PassMeta meta() {
        return passMeta< IndexFunctions >( "IndexFunctions", "Create function metadata tables" );
    }

    using lart::Pass::run;
    llvm::PreservedAnalyses run( llvm::Module &mod ) override {
        if ( !tagModuleWithMetadata( mod, "lart.divine.index.functions" ) )
            return llvm::PreservedAnalyses::all();

        llvm::GlobalVariable *mdRoot = mod.getGlobalVariable( "__md_functions" );
        ASSERT( mdRoot && "The bitcode must define __md_functions" );

        _funcMetaT = llvm::cast< llvm::StructType >( llvm::cast< llvm::ArrayType >(
                            mdRoot->getType()->getElementType() )->getElementType() );
        ASSERT( _funcMetaT && "The bitcode must define _MD_Function" );
        ASSERT( _funcMetaT->getNumElements() == 7 && "Incompatible _MD_Function" );

        _instMetaT = llvm::cast< llvm::StructType >( llvm::cast< llvm::PointerType >(
                            _funcMetaT->getElementType( 6 ) )->getElementType() );
        ASSERT( _instMetaT && "The bitcode must define _MD_InstInfo" );
        ASSERT( _instMetaT->getNumElements() == 2 && "Incompatible _MD_InstInfo" );

        llvm::GlobalVariable *mdSize = mod.getGlobalVariable( "__md_functions_count" );
        ASSERT( mdSize && "The bitcode must define __md_functions_count" );

        for ( auto &fn : mod ) {
            if ( fn.isIntrinsic() )
                continue;
            auto r = _meta.emplace( fn.getName(), FunctionMeta( fn, *this ) );
            ASSERT( r.second );
        }

        // build metadata table
        std::vector< llvm::Constant * > metatable;
        metatable.reserve( _meta.size() );
        for ( auto &m : _meta ) {
            std::string funNameStr = m.second.entryPoint->getName().str();
            std::string funNameCStr = funNameStr;
            funNameCStr.push_back( 0 );

            auto *instArrayT = llvm::ArrayType::get( _instMetaT, m.second.insts.size() );
            auto *instTable = new llvm::GlobalVariable( mod, instArrayT, true,
                                llvm::GlobalValue::ExternalLinkage,
                                llvm::ConstantArray::get( instArrayT, m.second.insts ),
                                "lart.divine.index.table." + funNameStr );

            auto *funNameA = llvm::ConstantDataArray::get( mod.getContext(),
                                llvm::ArrayRef< uint8_t >(
                                    reinterpret_cast< const uint8_t * >( funNameCStr.data() ),
                                    funNameCStr.size() ) );
            auto *funName = new llvm::GlobalVariable( mod, funNameA->getType(), true,
                                llvm::GlobalValue::ExternalLinkage,
                                funNameA, "lart.divine.index.name." + funNameStr );

            metatable.push_back( llvm::ConstantStruct::get( _funcMetaT, {
                    llvm::ConstantExpr::getPointerCast( funName, _funcMetaT->getElementType( 0 ) ),
                    llvm::ConstantExpr::getPointerCast( m.second.entryPoint,
                            _funcMetaT->getElementType( 1 ) ),
                    mkint( _funcMetaT, 2, m.second.frameSize ),
                    mkint( _funcMetaT, 3, m.second.argCount ),
                    mkint( _funcMetaT, 4, m.second.isVariadic ),
                    mkint( _funcMetaT, 5, m.second.insts.size() ),
                    llvm::ConstantExpr::getPointerCast( instTable,
                            llvm::PointerType::getUnqual(  _instMetaT ) )
                } ) );
        }

        // insert metadata table to the module
        mdSize->setInitializer( llvm::ConstantInt::get(
                    llvm::cast< llvm::PointerType >( mdSize->getType() )->getElementType(),
                    metatable.size() ) );

        auto *mdRootT = llvm::ArrayType::get( _funcMetaT, metatable.size() );
        auto *newMdRoot = new llvm::GlobalVariable( mod, mdRootT, true,
                    llvm::GlobalValue::ExternalLinkage,
                    llvm::ConstantArray::get( mdRootT, metatable ),
                    "lart.divine.index.functions", mdRoot );

        for ( auto u : query::query( mdRoot->users() ).freeze() ) { // avoid iterating over a list while we delete from it
            llvm::GetElementPtrInst *gep = llvm::dyn_cast< llvm::GetElementPtrInst >( u );
            llvm::ConstantExpr *constant = nullptr;
            if ( !gep ) {
                constant = llvm::cast< llvm::ConstantExpr >( u );
                gep = llvm::cast< llvm::GetElementPtrInst >(
                                        constant->getAsInstruction() );
            }
            std::vector< llvm::Value * > idxs;
            for ( auto &i : brick::query::range( gep->idx_begin(), gep->idx_end() ) )
                idxs.push_back( *&i );
            if ( constant ) {
                constant->replaceAllUsesWith( llvm::ConstantExpr::getGetElementPtr(
                                                  nullptr, newMdRoot, idxs ) );
                delete gep;
            } else
                llvm::ReplaceInstWithInst( gep, llvm::GetElementPtrInst::Create(
                                                 nullptr, newMdRoot, idxs ) );
        }
        auto mdRootName = mdRoot->getName().str();
        mdRoot->eraseFromParent();
        newMdRoot->setName( mdRootName );

        return llvm::PreservedAnalyses::none();
    }

    llvm::Constant *mkint( llvm::StructType *st, int i, int64_t val ) {
        return llvm::ConstantInt::get( st->getElementType( i ), val );
    }

    std::map< llvm::StringRef, FunctionMeta > _meta;
    llvm::StructType *_funcMetaT;
    llvm::StructType *_instMetaT;
};

PassMeta functionMetaPass() {
    return compositePassMeta< IndexFunctions >( "functionmeta",
            "Instrument bitcode with metadata of functions for DIVINE." );
}

}
}
