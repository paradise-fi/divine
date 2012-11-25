// -*- C++ -*- (c) 2012 Petr Rockai <me@mornfall.net>

#define NO_RTTI

#include <wibble/exception.h>

#include <divine/llvm/program.h>

#include <llvm/Type.h>
#include <llvm/GlobalVariable.h>
#include <llvm/CodeGen/IntrinsicLowering.h>
#include <llvm/Support/CallSite.h>
#include <llvm/Constants.h>
#include <llvm/Module.h>

using namespace divine::llvm;
using ::llvm::isa;
using ::llvm::dyn_cast;
using ::llvm::cast;

static bool isCodePointer( ::llvm::Value *val )
{
    return isa< ::llvm::Function >( val ) ||
           isa< ::llvm::BlockAddress >( val ) ||
           isa< ::llvm::BasicBlock >( val );
}

void ProgramInfo::initValue( ::llvm::Value *val, ProgramInfo::Value &result )
{
    result.width = TD.getTypeAllocSize( val->getType() );

    if ( val->getType()->isVoidTy() ) {
        result.width = 0;
        result.type = Value::Void;
    }

    if ( val->getType()->isPointerTy() ) {
        result.type = Value::Pointer;
        result.width = 4;
    }

    if ( val->getType()->isFloatTy() || val->getType()->isDoubleTy() )
        result.type = Value::Float;

    if ( isCodePointer( val ) ) {
        result.type = Value::CodePointer;
        result.width = 4;
    }
}

ProgramInfo::Value ProgramInfo::insert( int function, ::llvm::Value *val )
{
    if ( valuemap.find( val ) != valuemap.end() )
        return valuemap.find( val )->second;

    makeFit( functions, function );

    Value result; /* not seen yet, needs to be allocated */
    initValue( val, result );

    if ( auto B = dyn_cast< ::llvm::BasicBlock >( val ) )
        makeConstant( result, blockmap[ B ] );
    else if ( auto C = dyn_cast< ::llvm::Constant >( val ) ) {
        result.global = true;
        if ( auto G = dyn_cast< ::llvm::GlobalVariable >( val ) ) {
            assert( G->hasInitializer() ); /* extern globals are not allowed */
            if ( G->isConstant() )
                makeLLVMConstant( result, G->getInitializer() );
            else {
                Value pointee;
                initValue( G->getInitializer(), pointee );
                allocateValue( 0, pointee );
                globals.push_back( pointee );
                makeConstant( result, Pointer( false, globals.size() - 1, 0 ) );
            }
        } else makeLLVMConstant( result, C );
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
    IL->LowerIntrinsicCall( cast< ::llvm::CallInst >( p.I ) );
    if ( first ) insert = BB->begin();

    return Position( p.pc, insert );
}

void ProgramInfo::builtin( Position p )
{
    ProgramInfo::Instruction &insn = instruction( p.pc );
    ::llvm::CallSite CS( p.I );
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

    insn.opcode = p.I->getOpcode();
    insn.op = &*p.I;

    if ( dyn_cast< ::llvm::CallInst >( p.I ) ||
         dyn_cast< ::llvm::InvokeInst >( p.I ) )
    {
        ::llvm::CallSite CS( p.I );
        ::llvm::Function *F = CS.getCalledFunction();
        if ( F && F->isDeclaration() )
            switch ( F->getIntrinsicID() ) {
                case ::llvm::Intrinsic::not_intrinsic:
                case ::llvm::Intrinsic::trap:
                case ::llvm::Intrinsic::vastart:
                case ::llvm::Intrinsic::vacopy:
                case ::llvm::Intrinsic::vaend: break;
                case ::llvm::Intrinsic::dbg_declare:
                case ::llvm::Intrinsic::dbg_value: p.I++; return p;
                default: return lower( p );
            }
        if ( F->isDeclaration() )
            builtin( p );
    }

    insn.values.resize( 1 + p.I->getNumOperands() );
    for ( int i = 0; i < p.I->getNumOperands(); ++i ) {
        ::llvm::Value *v = p.I->getOperand( i );
        insert( p.pc.function, v ); /* use-before-def can actually happen */
        assert( valuemap.count( v ) );
        insn.values[ i + 1 ] = valuemap[ v ];
    }
    pcmap.insert( std::make_pair( p.I, p.pc ) );
    insn.result() = insert( p.pc.function, &*p.I );

    p.pc.instruction ++; p.I ++; /* next please */
    return p;
}

void ProgramInfo::build()
{
    PC pc( 0, 0, 0 );
    PC lastpc;

    /* null pointers are heap = 0, segment = 0, where segment is an index into
     * the "globals" vector */
    Value nullpage;
    nullpage.offset = 0;
    nullpage.width = 0;
    nullpage.global = true;
    nullpage.constant = false;
    globals.push_back( nullpage );

    for ( auto var = module->global_begin(); var != module->global_end(); ++ var )
        insert( 0, &*var );

    pc.function = 1;
    for ( auto function = module->begin(); function != module->end(); ++ function )
    {
        if ( function->isDeclaration() )
            continue; /* skip */
        ++ pc.function;

        if ( function->begin() == function->end() )
            throw wibble::exception::Consistency(
                "ProgramInfo::build",
                "Can't deal with empty functions." );

        functionmap[ function ] = pc.function;
        pc.block = 0;

        /* TODO: va_args; implement as a Pointer */
        for ( auto arg = function->arg_begin(); arg != function->arg_end(); ++ arg )
            insert( pc.function, &*arg );

        int blockid = 0;
        for ( auto block = function->begin(); block != function->end(); ++block, ++blockid )
            blockmap[ &*block ] = PC( pc.function, blockid, 0 );

        for ( auto block = function->begin(); block != function->end();
              ++ block, ++ pc.block )
        {
            if ( block->begin() == block->end() )
                throw wibble::exception::Consistency(
                    "ProgramInfo::build",
                    "Can't deal with an empty BasicBlock" );

            pc.instruction = 0;
            ProgramInfo::Position p( pc, block->begin() );
            while ( p.I != block->end() )
                p = insert( p );
            p = insert( p ); // end of block
        }

        align( this->function( pc ).framesize, 4 );
    }

    align( globalsize, 4 );
}

