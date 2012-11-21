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
#include <divine/llvm/interpreter.h>
#include "llvm/CodeGen/IntrinsicLowering.h"
#include "llvm/DerivedTypes.h"
#include "llvm/Constants.h"
#include "llvm/Module.h"
#include <cstring>
using namespace llvm;
using namespace divine::llvm;

static void handleBB( ProgramInfo *info, ProgramInfo::Value &result, ::llvm::BasicBlock *b )
{
    assert( info->blockmap.count( b ) );
    info->storeConstant( result, info->blockmap[ b ] );
}

ProgramInfo::Value ProgramInfo::insert( int function, ::llvm::Value *val )
{
    if ( valuemap.find( val ) != valuemap.end() )
        return valuemap.find( val )->second;

    makeFit( functions, function );

    Value result; /* not seen yet, needs to be allocated */
    result.width = target.getTypeAllocSize( val->getType() );
    if ( val->getType()->isVoidTy() )
        result.width = 0;

    if ( val->getType()->isPointerTy() ) {
        result.pointer = !isa< ::llvm::Function >( val ) &&
                         !isa< ::llvm::BlockAddress >( val ) &&
                         !isa< ::llvm::BasicBlock >( val );
        result.width = 4;
    }

    if ( auto F = dyn_cast< ::llvm::Function >( val ) )
        storeConstant( result, PC( functionmap[ F ], 0, 0 ) );
    else if ( auto B = dyn_cast< BlockAddress >( val ) )
        handleBB( this, result, B->getBasicBlock() );
    else if ( auto B = dyn_cast< BasicBlock >( val ) )
        handleBB( this, result, B );
    else if ( auto C = dyn_cast< Constant >( val ) ) {
        if ( result.pointer ) {
            result.global = true;
            if ( auto G = dyn_cast< GlobalVariable >( val ) ) {
                if ( G->isConstant() ) {
                    assert( G->hasInitializer() );
                    storeConstant( result, interpreter.getConstantValue( G->getInitializer() ),
                                   C->getType() );
                } else allocateValue( 0, result );
            } else assert_die(); /* duh. */
        } else storeConstant( result, interpreter.getConstantValue( C ), C->getType() );
    } else allocateValue( function, result );

    if ( function && !result.global && !result.constant ) {
        if ( result.width )
            this->functions[ function ].values.push_back( result );
    } else
        globals.push_back( result );

    valuemap.insert( std::make_pair( val, result ) );
    return result;
}

/* This is silly. */
ProgramInfo::Position ProgramInfo::lower( Position p )
{
    auto BB = p.I->getParent();
    bool first = p.I == BB->begin();
    auto insert = p.I;

    if ( !first ) --insert;
    interpreter.IL->LowerIntrinsicCall( cast< CallInst >( p.I ) );
    if ( first ) insert = BB->begin();

    return Position( p.pc, insert );
}

void ProgramInfo::builtin( Position p )
{
    ProgramInfo::Instruction &insn = instruction( p.pc );
    CallSite CS( p.I );
    ::llvm::Function *F = CS.getCalledFunction();
    std::string name = F->getName().str();
    if ( name == "__divine_interrupt_mask" )
        insn.builtin = BuiltinMask;
    else if ( name == "__divine_interrupt_unmask" )
        insn.builtin = BuiltinUnmask;
    else if ( name == "__divine_get_tid" )
        insn.builtin = BuiltinGetTID;
    else if ( name == "__divine_new_thread" )
        insn.builtin = BuiltinNewThread;
    else if ( name == "__divine_choice" )
        insn.builtin = BuiltinChoice;
    else throw wibble::exception::Consistency(
        "ProgramInfo::builtin",
        "Can't call an undefined function <" + name + ">" );
}

ProgramInfo::Position ProgramInfo::insert( Position p )
{
    makeFit( functions, p.pc.function );
    makeFit( function( p.pc ).blocks, p.pc.block );
    makeFit( function( p.pc ).block( p.pc ).instructions, p.pc.instruction );
    ProgramInfo::Instruction &insn = instruction( p.pc );

    if ( !block( p.pc ).bb )
        block( p.pc ).bb = p.I->getParent();

    if ( p.I == block( p.pc ).bb->end() )
        return p; /* nowhere further to go */
    insn.op = &*p.I;

    if ( dyn_cast< CallInst >( p.I ) || dyn_cast< InvokeInst >( p.I ) ) {
        CallSite CS( p.I );
        ::llvm::Function *F = CS.getCalledFunction();
        if ( F && F->isDeclaration() )
            switch ( F->getIntrinsicID() ) {
                case Intrinsic::not_intrinsic:
                case Intrinsic::trap:
                case Intrinsic::vastart:
                case Intrinsic::vacopy:
                case Intrinsic::vaend: break;
                case Intrinsic::dbg_declare:
                case Intrinsic::dbg_value: p.I++; return p;
                default: return lower( p );
            }
        if ( F->isDeclaration() )
            builtin( p );
    }

    insn.operands.resize( p.I->getNumOperands() );
    for ( int i = 0; i < p.I->getNumOperands(); ++i ) {
        ::llvm::Value *v = p.I->getOperand( i );
        insert( p.pc.function, v ); /* use-before-def can actually happen */
        assert( valuemap.count( v ) );
        insn.operands[ i ] = valuemap[ v ];
    }
    pcmap.insert( std::make_pair( p.I, p.pc ) );
    insn.result = insert( p.pc.function, &*p.I );

    p.pc.instruction ++; p.I ++; /* next please */
    return p;
}

void ProgramInfo::storeGV( char *where, GenericValue GV, Type *ty, int width ) {
    if ( ty->isIntegerTy() ) { /* StoreValueToMemory is buggy for (at least) integers... */
        const uint8_t *mem = reinterpret_cast< const uint8_t * >( GV.IntVal.getRawData() );
        std::copy( mem, mem + width, where );
    } else {
        interpreter.StoreValueToMemory(
            GV, reinterpret_cast< GenericValue * >( where ), ty );
    }
}

void ProgramInfo::storeConstant( Value &result, GenericValue GV, Type *ty )
{
    result.constant = true;
    result.width = target.getTypeStoreSize( ty );
    storeGV( allocateConstant( result ), GV, ty, result.width );
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
        info.functionmap[ function ] = pc.function;
        pc.block = 0;

        if ( function->begin() == function->end() )
            throw wibble::exception::Consistency(
                "Interpreter::buildInfo",
                "Can't deal with empty functions." );

        /* TODO: va_args; implement as a Pointer */
        for ( auto arg = function->arg_begin(); arg != function->arg_end(); ++ arg )
            info.insert( pc.function, &*arg );

        int blockid = 0;
        for ( auto block = function->begin(); block != function->end(); ++block, ++blockid )
            info.blockmap[ &*block ] = PC( pc.function, blockid, 0 );

        for ( auto block = function->begin(); block != function->end();
              ++ block, ++ pc.block )
        {
            if ( block->begin() == block->end() )
                throw wibble::exception::Consistency(
                    "Interpreter::buildInfo",
                    "Can't deal with an empty BasicBlock" );

            pc.instruction = 0;
            ProgramInfo::Position p( pc, block->begin() );
            while ( p.I != block->end() )
                p = info.insert( p );
            p = info.insert( p ); // end of block
        }

        auto &fun = info.function( pc );
        fun.framesize = 0;
        for ( auto v = fun.values.begin(); v != fun.values.end(); ++v )
            fun.framesize += v->width;
        align( fun.framesize, 4 ); // 4-byte align please
    }

    align( info.globalsize, 4 );
}

divine::Blob Interpreter::initial( Function *f )
{
    Blob pre_initial = alloc.new_blob( state.size( 0, 0, 0 ) );
    pre_initial.clear();
    state.rewind( pre_initial, 0 ); // there isn't a thread really

    int idx = 0;
    for ( auto var = module->global_begin(); var != module->global_end(); ++ var, ++ idx ) {
        auto val = info.globals[ idx ];
        if ( var->hasInitializer() )
            info.storeGV( state.dereference( val ),
                          getConstantValue( var->getInitializer() ),
                          var->getType(), val.width );
    }

    int tid = state.new_thread(); // switches automagically
    assert_eq( tid, 0 ); // just to be on the safe side...
    state.enter( info.functionmap[ f ] );
    Blob result = state.snapshot();
    state.rewind( result, 0 ); // so that we don't wind up in an invalid state...
    // pre_initial got free'd by MachineState since it wasn't permanent
    return result;
}

void Interpreter::new_thread( Function *f )
{
    int tid = state.new_thread();
    state.enter( info.functionmap[ f ] );
    state.rewind( state.snapshot(), tid );
}
