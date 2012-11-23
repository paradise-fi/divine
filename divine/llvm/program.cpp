// -*- C++ -*- (c) 2012 Petr Rockai <me@mornfall.net>

#define NO_RTTI
#include <wibble/exception.h>

#include <divine/llvm/program.h>
#include <divine/llvm/interpreter.h>

#include <llvm/Type.h>
#include <llvm/GlobalVariable.h>
#include <llvm/CodeGen/IntrinsicLowering.h>
#include <llvm/Constants.h>

using namespace divine::llvm;
using ::llvm::isa;
using ::llvm::dyn_cast;

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
    else if ( auto B = dyn_cast< ::llvm::BlockAddress >( val ) )
        handleBB( this, result, B->getBasicBlock() );
    else if ( auto B = dyn_cast< ::llvm::BasicBlock >( val ) )
        handleBB( this, result, B );
    else if ( auto C = dyn_cast< ::llvm::Constant >( val ) ) {
        if ( result.pointer ) {
            result.global = true;
            if ( auto G = dyn_cast< ::llvm::GlobalVariable >( val ) ) {
                if ( G->isConstant() ) {
                    assert( G->hasInitializer() );
                    storeConstant( result, interpreter.getConstantValue( G->getInitializer() ),
                                   C->getType() );
                } else allocateValue( 0, result );
            } else storeConstant( result, interpreter.getConstantValue( C ), C->getType() );
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

    if ( dyn_cast< ::llvm::CallInst >( p.I ) ||
         dyn_cast< ::llvm::InvokeInst >( p.I ) )
    {
        CallSite CS( p.I );
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

void ProgramInfo::storeGV( char *where, GenericValue GV, Type *ty, int width )
{
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

void ProgramInfo::build( Module *module )
{
    PC pc( 0, 0, 0 );
    PC lastpc;

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
                "Interpreter::buildInfo",
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
                    "Interpreter::buildInfo",
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

