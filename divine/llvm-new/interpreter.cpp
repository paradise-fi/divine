//===- Interpreter.cpp - Top-Level LLVM Interpreter Implementation --------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements the top-level functionality for the LLVM interpreter.
// This interpreter is designed to be a very simple, portable, inefficient
// interpreter.
//
//===----------------------------------------------------------------------===//

#define NO_RTTI
#include <wibble/exception.h>
#include <divine/llvm-new/interpreter.h>
#include "llvm/CodeGen/IntrinsicLowering.h"
#include "llvm/DerivedTypes.h"
#include "llvm/Constants.h"
#include "llvm/Module.h"
#include <cstring>
using namespace llvm;
using namespace divine::llvm2;

ProgramInfo::Value ProgramInfo::insert( int function, ::llvm::Value *val )
{
    if ( valuemap.find( val ) != valuemap.end() )
        return valuemap.find( val )->second;

    Value result; /* not seen yet, needs to be allocated */
    auto C = dyn_cast< Constant >( val );

    if ( C && C->getType()->getTypeID() == Type::PointerTyID )
    {
        if ( auto F = dyn_cast< ::llvm::Function >( val ) )
            storeConstant( result, PC( functionmap[ F ], 0, 0 ) );
        if ( auto B = dyn_cast< BlockAddress >( val ) )
            storeConstant( result, PC( functionmap[ B->getFunction() ],
                                       blockmap[ B->getBasicBlock() ], 0 ) );
        if ( auto G = dyn_cast< GlobalVariable >( val ) ) {
            if ( G->isConstant() )
                storeConstant( result, interpreter.getConstantValue( C ), C->getType() );
            else {
                result.constant = true;
                result.offset = globalsize;
                result.width = target.getTypeAllocSize( C->getType() );
                globalsize += result.width;
                /* TODO what about vector types? */
            }
        }
        result.global = true;
    }

    if (function) {
        // must be already there... makeFit( functions, function );
        this->functions[ function ].values.push_back( result );
    } else {
        globals.push_back( result );
    }

    valuemap.insert( std::make_pair( val, result ) );
    return result;
}

void ProgramInfo::insert( PC pc, ::llvm::Instruction *i )
{
    makeFit( functions, pc.function );
    makeFit( function( pc ).blocks, pc.block );
    makeFit( function( pc ).block( pc ).instructions, pc.instruction );
    // instruction( pc ).values
    pcmap.insert( std::make_pair( i, pc ) );
}

void ProgramInfo::storeConstant( Value &result, GenericValue GV, Type *ty )
{
    interpreter.StoreValueToMemory(
        GV, reinterpret_cast< GenericValue * >(
            allocateConstant( result, target.getTypeStoreSize( ty ) ) ), ty );
}

Interpreter::Interpreter(Allocator &alloc, Module *M)
    : ExecutionEngine(M), TD(M), module( M ), info( TD, *this ), alloc( alloc ), state( info, alloc )
{
    setTargetData(&TD);
    initializeExecutionEngine();
    IL = new IntrinsicLowering(TD);
    parseProperties( M );
    buildInfo( M );
}

Interpreter::~Interpreter() {
  delete IL;
}

void Interpreter::parseProperties( Module *M )
{
    for (Module::const_global_iterator var = M->global_begin(); var != M->global_end(); ++var)
    {
        if ( var->isConstant() )
            continue;

        // GlobalVariable type is always a pointer, so dereference it first
        Type *ty = var->getType()->getTypeAtIndex(unsigned(0));
        assert( ty );

        assert( !var->isDeclaration() ); // can't handle extern's yet
        if ( std::string( var->getName().str(), 0, 6 ) == "__LTL_" ) {
            std::string name( var->getName().str(), 6, std::string::npos );
            GenericValue GV = getConstantValue(var->getInitializer());
            properties[name] = std::string( (char*) GV.PointerVal );
            continue; // do not emit this one
        }
    }
}

void Interpreter::buildInfo( Module *module ) {
    PC pc( 0, 0, 0 );
    PC lastpc;

    for ( auto var = module->global_begin(); var != module->global_end(); ++ var )
        info.insert( 0, &*var );

    pc.function = 1;
    for ( auto function = module->begin(); function != module->end();
          ++ function, ++ pc.function )
    {
        pc.block = 0;

        if ( function->begin() == function->end() )
            throw wibble::exception::Consistency(
                "Interpreter::buildInfo",
                "Can't deal with empty functions." );

        for ( auto arg = function->arg_begin(); arg != function->arg_end(); ++ arg )
            info.insert( pc.function, &*arg );

        for ( auto block = function->begin(); block != function->end();
              ++ block, ++ pc.block )
        {
            if ( block->begin() == block->end() )
                throw wibble::exception::Consistency(
                    "Interpreter::buildInfo",
                    "Can't deal with an empty BasicBlock" );

            pc.instruction = 0;
            for ( auto insn = block->begin(); insn != block->end();
                  ++ insn, ++ pc.instruction )
            {
                info.insert( pc, &*insn );
                info.insert( pc.function, &*insn );
            }
            info.insert( pc, nullptr ); /* past-the-end iterator, for BB end */
        }

        auto &fun = info.function( pc );
        fun.framesize = 0;
        for ( auto v = fun.values.begin(); v != fun.values.end(); ++v )
            fun.framesize += v->width;
    }
}

divine::Blob Interpreter::initial( Function *f )
{
    int emptysize = sizeof( MachineState::Flags ) + // flags...
                    info.globalsize + // globals
                    sizeof( int ); // threads array length
    Blob pre_initial = alloc.new_blob( emptysize );
    pre_initial.clear();
    state.rewind( pre_initial, 0 ); // there isn't a thread really
    int tid = state.new_thread(); // switches automagically
    assert_eq( tid, 0 ); // just to be on the safe side...
    state.enter( info.functionmap[ f ] );
    Blob result = state.snapshot();
    state.rewind( result, 0 ); // so that we don't wind up in an invalid state...
    pre_initial.free( alloc.pool() ); // wee
    return result;
}
