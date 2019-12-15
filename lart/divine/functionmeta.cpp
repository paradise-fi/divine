// -*- C++ -*- (c) 2016 Vladimír Štill <xstill@fi.muni.cz>

DIVINE_RELAX_WARNINGS
#include <llvm/Pass.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
DIVINE_UNRELAX_WARNINGS

#include <brick-string>
#include <brick-llvm>
#include <string>
#include <iostream>

#include <lart/support/pass.h>
#include <lart/support/meta.h>
#include <lart/support/query.h>
#include <lart/support/util.h>
#include <lart/support/metadata.h>
#include <lart/support/annotate.h>
#include <lart/divine/metadata.h>

#include <divine/vm/divm.h>

/*
 * This file contains the `IndexFunctions` pass that is responsible for
 * metadata generation.
 *
 * Here, we see the origin of most of the metadata used by DiOS. Before
 * the execution of the provided bitcode begins, it is scanned and transformed
 * several times. This pass scans a given module, extracts information about
 * functions and globals, and injects metadata representing it back into the
 * bitcode.
 *
 * It works as follows: The pass scans the input bitcode and builds an internal
 * representation of its functions and globals (structs `FunctionMeta` and
 * `GlobalMeta`). From this data, it generates arrays of type `_MD_Function`
 * and `_MD_Global` which represent the metadata at runtime. These arrays are
 * then hooked back into the bitcode by inserting them as initializers of
 * global variables `__md_functions` and `__md_globals`. The pass also stores
 * the sizes of the aforementioned arrays in `__md_functions_count` and
 * `__md_globals_count`.
 */

namespace lart {
namespace divine {

/*
 * Encode a single LLVM type as a character.
 * Partially implements Itanium ABI C++ mangling.
 * Pointer types are not distinguished from each other.
 *
 * +------------+------+
 * |    type    | char |
 * +------------+------+
 * | bool       | b    |
 * | char       | c    |
 * | int16_t    | s    |
 * | int32_t    | i    |
 * | int64_t    | l    |
 * | __int128_t | n    |
 * | T *        | p    |
 * | void       | v    |
 * +------------+------+
 */

char encLLVMBasicType( llvm::Type *t ) {
    if ( t->isVoidTy() )
        return metadata::TypeSig::encodeOne< void >;
    if ( t->isPointerTy() )
        return metadata::TypeSig::encodeOne< void * >;
    if ( auto *it = llvm::dyn_cast< llvm::IntegerType >( t ) )
        return metadata::TypeSig::encodeIntTy( it->getBitWidth() );
    return metadata::TypeSig::unknownType;
}

/*
 * Encode a function type as a string.
 * The first character is the return type, the parameters follow after it.
 */

std::string encLLVMFunType( llvm::FunctionType *ft ) {
    std::string enc;
    enc.push_back( encLLVMBasicType( ft->getReturnType() ) );
    for ( auto t : ft->params() )
        enc.push_back( encLLVMBasicType( t ) );
    return enc;
}

struct FunctionMeta {
    explicit FunctionMeta( llvm::Function &fn ) :
        entryPoint( &fn ),
        frameSize( 0 ), // re-written by the loader
        argCount( fn.arg_size() ),
        isVariadic( fn.isVarArg() ),
        type( encLLVMFunType( fn.getFunctionType() ) ),
        // divine has 1 pc value for every instruction + one special at the
        // beginning of each basic block
        instTableSize( query::query( fn ).fold( 0,
                        []( int x, llvm::BasicBlock &bb ) { return x + bb.size() + 1; } ) )
    { }

    template< typename Index >
    static int argsSize( llvm::Function &fn, Index &index ) {
        auto tys = fn.getFunctionType()->params();
        return std::accumulate( tys.begin(), tys.end(), 0, [&]( int accum, llvm::Type *t ) {
                return accum + index._dl->getTypeStoreSize( t );
            } );
    }

    llvm::Function *entryPoint;
    int frameSize;
    int argCount;
    bool isVariadic;
    std::string type;
    int instTableSize;
};

struct GlobalMeta {
    template< typename Index >
    GlobalMeta( llvm::GlobalVariable &g, Index &index ) :
        address( &g ), size( index._dl->getTypeStoreSize(
                    llvm::cast< llvm::PointerType >( g.getType() )->getElementType() ) )
    { }

    llvm::GlobalVariable *address;
    long size;
    bool isConstant() const { return address->isConstant(); }
    llvm::StringRef name() const { return address->getName(); }
};

struct IndexFunctions {

    inline static const std::string trapfn = "divine.trapfn";

    static PassMeta meta() {
        return passMeta< IndexFunctions >( "IndexFunctions", "Create function metadata tables" );
    }

    void run( llvm::Module &mod )
    {
	// Tag this module as processed, skip if already tagged
        if ( !tagModuleWithMetadata( mod, "lart.divine.index.functions" ) )
            return;

        _dl = std::make_unique< llvm::DataLayout >( &mod );

	// Set up injection points
        llvm::GlobalVariable *mdRoot = mod.getGlobalVariable( "__md_functions" );
	// Types of _MD_Function, _MD_InstInfo, and _MD_Global
        llvm::StructType *funcMetaT = nullptr, *instMetaT = nullptr, *gloMetaT = nullptr;


        if ( mdRoot )
        {
            funcMetaT = llvm::cast< llvm::StructType >(
                            llvm::cast< llvm::ArrayType >(
                                mdRoot->getType()->getElementType() )->getElementType() );
            ASSERT( funcMetaT && "The bitcode must define _MD_Function" );
            ASSERT( funcMetaT->getNumElements() == 12 && "Incompatible _MD_Function" );

            instMetaT = llvm::cast< llvm::StructType >(
                            llvm::cast< llvm::PointerType >(
                                funcMetaT->getElementType( 7 ) )->getElementType() );
            ASSERT( instMetaT && "The bitcode must define _MD_InstInfo" );
        }

        llvm::GlobalVariable *mdSize = mod.getGlobalVariable( "__md_functions_count" );
        if ( mdRoot )
            ASSERT( mdSize && "The bitcode must define __md_functions_count" );

        llvm::GlobalVariable *mdGlobals = mod.getGlobalVariable( "__md_globals" );

        if ( mdGlobals )
        {
            gloMetaT = llvm::cast< llvm::StructType >(
                           llvm::cast< llvm::ArrayType >(
                               mdGlobals->getType()->getElementType() )->getElementType() );
            ASSERT( gloMetaT && "The bitcode must define _MD_Global" );
            ASSERT( gloMetaT->getNumElements() == 4 && "Incompatible _MD_Global" );
        }

        llvm::GlobalVariable *mdGlobalsCount = mod.getGlobalVariable( "__md_globals_count" );
        if ( mdGlobals )
            ASSERT( mdGlobalsCount && "The bitcode must define __md_globals_count" );

	// Look up functions marked with `__trapfn`
        LowerAnnotations( trapfn ).run( mod );
        auto traps = util::functions_with_attr( trapfn, mod );

        std::vector< FunctionMeta > funMeta;

	// Build internal representation for every defined function
        if ( mdRoot )
            for ( auto &fn : mod )
                if ( !fn.isDeclaration() )
                    funMeta.emplace_back( fn );

        // build metadata table
        std::vector< llvm::Constant * > metatable;
        metatable.reserve( funMeta.size() );

        for ( auto &m : funMeta )
        {
            std::string funNameStr = m.entryPoint->getName().str();
            std::string funNameCStr = funNameStr;
            funNameCStr.push_back( 0 );

            auto *funName = util::getStringGlobal( funNameCStr, mod );
            funName->setName( "lart.divine.index.name." + funNameStr );
            auto type = m.type;
            type.push_back( 0 );
            auto *funType = util::getStringGlobal( type, mod );
            auto *persT = llvm::cast< llvm::PointerType >( funcMetaT->getElementType( 8 ) );
            auto *pers = m.entryPoint->hasPersonalityFn()
                            ? llvm::ConstantExpr::getPointerCast(
                                    m.entryPoint->getPersonalityFn(), persT )
                            : llvm::ConstantPointerNull::get( persT );

            auto *lsdaT = llvm::cast< llvm::PointerType >( funcMetaT->getElementType( 9 ) );
            auto *lsdam = m.entryPoint->getMetadata( "lart.lsda" );
            auto *lsda = lsdam
                            ? llvm::cast< llvm::ConstantAsMetadata >( lsdam->getOperand( 0 ) )->getValue()
                            : llvm::ConstantPointerNull::get( lsdaT );

            metatable.push_back( llvm::ConstantStruct::get( funcMetaT, {
                    llvm::ConstantExpr::getPointerCast( funName, funcMetaT->getElementType( 0 ) ),
                    llvm::ConstantExpr::getPointerCast( m.entryPoint,
                            funcMetaT->getElementType( 1 ) ),
                    mkint( funcMetaT, 2, m.frameSize ),
                    mkint( funcMetaT, 3, m.argCount ),
                    mkint( funcMetaT, 4, m.isVariadic ),
                    llvm::ConstantExpr::getPointerCast( funType,
                                            funcMetaT->getElementType( 5 ) ),
                    mkint( funcMetaT, 6, m.instTableSize ),
                    llvm::ConstantPointerNull::get( llvm::PointerType::getUnqual( instMetaT ) ),
                    pers,
                    lsda,
                    mkint( funcMetaT, 10, m.entryPoint->hasFnAttribute( llvm::Attribute::NoUnwind ) ),
                    mkint( funcMetaT, 11, traps.count( m.entryPoint ) )
                } ) );
        }

        // insert metadata table to the module
        if ( mdRoot )
        {
            mdSize->setInitializer( llvm::ConstantInt::get(
                        llvm::cast< llvm::PointerType >( mdSize->getType() )->getElementType(),
                        metatable.size() ) );

            auto *mdRootT = llvm::ArrayType::get( funcMetaT, metatable.size() );
            util::replaceGlobalArray( mdRoot, llvm::ConstantArray::get( mdRootT, metatable ) );
        }

        // now build global variable metadata
        std::map< llvm::StringRef, GlobalMeta > gloMetaMap;

        if ( mdGlobals )
            for ( auto &g : mod.globals() )
            {
                if ( g.hasName() && ( brq::starts_with( g.getName().str(), "llvm." ) ||
                                      brq::starts_with( g.getName().str(), "__dios_" ) ) )
                {
                    auto r = gloMetaMap.emplace( g.getName(), GlobalMeta( g, *this ) );
                    if ( !r.second )
                        UNREACHABLE( "broken metadata map: global", g, "already present" );
                }
            }

        std::vector< llvm::Constant * > glometa;

        for ( auto &m : gloMetaMap )
        {
            std::string name = m.second.name().str();
            name.push_back( 0 );
            auto *llvmname = util::getStringGlobal( name, mod );
            llvmname->setName( "lart.divine.index.globalname." + m.second.name().str() );

            glometa.push_back( llvm::ConstantStruct::get( gloMetaT, {
                        llvm::ConstantExpr::getPointerCast( llvmname,
                                                            gloMetaT->getElementType( 0 ) ),
                        llvm::ConstantExpr::getPointerCast( m.second.address,
                                                            gloMetaT->getElementType( 1 ) ),
                        mkint( gloMetaT, 2, m.second.size ),
                        mkint( gloMetaT, 3, m.second.isConstant() )
                    } ) );
        }

	// Insert global variable metadata into the module
        if ( mdGlobals )
        {
            mdGlobalsCount->setInitializer( llvm::ConstantInt::get(
                        llvm::cast< llvm::PointerType >( mdGlobalsCount->getType() )->getElementType(),
                        glometa.size() ) );
            auto *gloArrayT = llvm::ArrayType::get( gloMetaT, glometa.size() );
            util::replaceGlobalArray( mdGlobals, llvm::ConstantArray::get( gloArrayT, glometa ) );
        }
    }

    llvm::Constant *mkint( llvm::StructType *st, int i, int64_t val ) {
        return llvm::ConstantInt::get( st->getElementType( i ), val );
    }

    std::unique_ptr< llvm::DataLayout > _dl;
};

PassMeta functionMetaPass() {
    return compositePassMeta< IndexFunctions >( "functionmeta",
            "Instrument bitcode with metadata of functions for DIVINE." );
}

}
}
