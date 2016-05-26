// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2012-2016 Petr Ročkai <code@fixp.eu>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdexcept>

#include <divine/vm/program.hpp>

#include <llvm/IR/Type.h>
#include <llvm/IR/GlobalVariable.h>

#include <llvm/CodeGen/IntrinsicLowering.h>
#include <llvm/IR/CallSite.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Module.h>
#include <llvm/ADT/StringMap.h>

using namespace divine::vm;
using llvm::isa;
using llvm::dyn_cast;
using llvm::cast;

CodePointer Program::functionByName( std::string s )
{
    llvm::Function *f = module->getFunction( s );
    if ( !f )
        return CodePointer();
    return functionmap[ f ];
}

bool Program::isCodePointerConst( llvm::Value *val )
{
    return ( isa< llvm::Function >( val ) ||
             isa< llvm::BlockAddress >( val ) ||
             isa< llvm::BasicBlock >( val ) );
}

bool Program::isCodePointer( llvm::Value *val )
{
    if ( auto ty = dyn_cast< llvm::PointerType >( val->getType() ) )
        return ty->getElementType()->isFunctionTy();

    return isCodePointerConst( val );
}

CodePointer Program::getCodePointer( llvm::Value *val )
{
    if ( auto B = dyn_cast< llvm::BasicBlock >( val ) ) {
        ASSERT( blockmap.count( B ) );
        return blockmap[ B ];
    } else if ( auto B = dyn_cast< llvm::BlockAddress >( val ) ) {
        ASSERT( blockmap.count( B->getBasicBlock() ) );
        return blockmap[ B->getBasicBlock() ];
    } else if ( auto F = dyn_cast< llvm::Function >( val ) ) {
        if ( !functionmap.count( F ) && builtin( F ) == NotBuiltin )
            throw std::logic_error(
                "Program::insert: " +
                std::string( "Unresolved symbol (function): " ) + F->getName().str() );

        if ( builtin( F ) )
            return CodePointer();

        return CodePointer( functionmap[ F ], 0 );
    }
    return CodePointer();
}

namespace {

int intrinsic_id( llvm::Value *v ) {
    auto insn = dyn_cast< llvm::Instruction >( v );
    if ( !insn || insn->getOpcode() != llvm::Instruction::Call )
        return llvm::Intrinsic::not_intrinsic;
    llvm::CallSite CS( insn );
    auto f = CS.getCalledFunction();
    if ( !f )
        return llvm::Intrinsic::not_intrinsic;
    return f->getIntrinsicID();
}

}

Program::Slot Program::initSlot( llvm::Value *val, Slot::Location loc )
{
    Slot result;
    result.location = loc;

    if ( val->getType()->isVoidTy() ) {
        result.width = 0;
        result.type = Slot::Void;
    } else if ( isCodePointer( val ) ) {
        result.width = val->getType()->isPointerTy() ? TD.getTypeAllocSize( val->getType() )
                       : sizeof( CodePointer );
        result.type = Slot::CodePointer;
    } else if ( val->getType()->isPointerTy() ) {
        result.width = TD.getTypeAllocSize( val->getType() );
        result.type = Slot::Pointer;
    } else {
        result.width = TD.getTypeAllocSize( val->getType() );
        if ( val->getType()->isIntegerTy() )
            result.type = Slot::Integer;
        else if ( val->getType()->isFloatTy() || val->getType()->isDoubleTy() )
            result.type = Slot::Float;
        else
            result.type = Slot::Aggregate;
    }

    if ( auto CDS = dyn_cast< llvm::ConstantDataSequential >( val ) ) {
        result.type = Slot::Aggregate;
        result.width = CDS->getNumElements() * CDS->getElementByteSize();
    }

    if ( isa< llvm::AllocaInst >( val ) ||
         intrinsic_id( val ) == llvm::Intrinsic::stacksave )
    {
        ASSERT_EQ( loc, Slot::Local );
        result.type = Program::Slot::Alloca;
    }
    return result;
}

bool Program::lifetimeOverlap( llvm::Value *v_a, llvm::Value *v_b ) {
    auto a = dyn_cast< llvm::Instruction >( v_a );
    auto b = dyn_cast< llvm::Instruction >( v_b );
    if ( !a || !b )
        return true;

    auto id = a->getMetadata( "lart.id" );
    auto list = b->getMetadata( "lart.interference" );

    if ( !list || !id )
        return true;
    for ( int i = 0; i < list->getNumOperands(); ++i ) {
        auto item = dyn_cast< llvm::MDNode >( list->getOperand( i ) );
        if ( item == id )
            return true;
    }
    return false;
}

void Program::overlaySlot( int fun, Slot &result, llvm::Value *val )
{
    if ( !result.width )
    {
        result.offset = 0;
        return;
    }

    int iter = 0;
    auto &f = functions[ fun ];
    makeFit( coverage, fun );
    auto &c = coverage[ fun ];
    llvm::Instruction *insn;

    if ( !val )
        goto alloc;

    insn = dyn_cast< llvm::Instruction >( val );
    if ( !insn || !insn->getMetadata( "lart.interference" ) )
        goto alloc;

    ASSERT( !f.instructions.empty() );
    c.resize( f.datasize );

    for ( result.offset = 0; result.offset < f.datasize; ) {
        iter ++;
        if ( result.offset + result.width > f.datasize )
            goto alloc;

        bool good = true;
        for ( int i = 0; good && i < result.width; ++i )
            for ( auto v : c[ result.offset + i ] )
                if ( lifetimeOverlap( val, v ) ) {
                    auto s = valuemap[ v ].slot;
                    result.offset = s.offset + s.width;
                    good = false;
                    break;
                }

        if ( good )
            goto out;
    }
alloc:
    result.offset = f.datasize;
    f.datasize += result.width;
    c.resize( f.datasize );
out:
    c[ result.offset ].push_back( val );
}

Program::SlotRef Program::insert( int function, llvm::Value *val )
{
    /* global scope is only applied to GlobalVariable initializers */
    if ( !function )
        return insert( 0, val, Slot::Constant );
    if ( isa< llvm::Constant >( val ) || isCodePointer( val ) )
        return insert( function, val, Slot::Constant );
    return insert( function, val, Slot::Local );
}

Program::SlotRef Program::insert( int function, llvm::Value *val, Slot::Location sl )
{
    if ( auto GA = dyn_cast< llvm::GlobalAlias >( val ) )
    {
        auto r = insert( function, const_cast< llvm::GlobalObject * >( GA->getBaseObject() ), sl );
        valuemap.insert( std::make_pair( val, r ) );
        return r;
    }

    if ( valuemap.find( val ) != valuemap.end() )
        return valuemap.find( val )->second;

    makeFit( functions, function );

    auto slot = initSlot( val, sl );

    if ( slot.width % framealign )
        return SlotRef(); /* ignore for now, later pass will assign this one */

    auto sref = allocateSlot( slot, function, val );

    if ( auto G = dyn_cast< llvm::GlobalVariable >( val ) )
    {
        /* G is a pointer type, G's initializer is of the actual value type  */
        if ( !G->hasInitializer() )
            throw std::logic_error(
                "Program::insert: " +
                std::string( "Unresolved symbol (global variable): " ) +
                G->getValueName()->getKey().str() );

        auto pointee = insert( 0, G->getInitializer(),
                               G->isConstant() ? Slot::Constant : Slot::Global );
        auto p = s2ptr( pointee );
        _toinit.emplace_back( [=]{
                initStatic( sref.slot, value::Pointer<>( p ) );
                _doneinit.insert( G );
            } );
    }
    else if ( isa< llvm::Constant >( val ) || isCodePointer( val ) )
        _toinit.emplace_back( [=]{ initStatic( sref.slot, val ); } );

    valuemap.insert( std::make_pair( val, sref ) );

    if ( !isa< llvm::GlobalVariable >( val ) )
        if ( auto U = dyn_cast< llvm::User >( val ) )
            for ( int i = 0; i < int( U->getNumOperands() ); ++i )
                insert( function, U->getOperand( i ) );

    return sref;
}

/* This is silly. */
Program::Position Program::lower( Position p )
{
    auto BB = p.I->getParent();
    bool first = p.I == BB->begin();
    auto insert = p.I;

    if ( !first ) --insert;
    IL->LowerIntrinsicCall( cast< llvm::CallInst >( p.I ) );
    if ( first )
        insert = BB->begin();
    else
        insert ++;

    return Position( p.pc, insert );
}

Builtin Program::builtin( llvm::Function *f )
{
    std::string name = f->getName().str();

    if ( name == "__vm_set_sched" )
        return BuiltinSetSched;
    if ( name == "__vm_set_fault" )
        return BuiltinSetFault;
    if ( name == "__vm_set_ifl" )
        return BuiltinSetIfl;

    if ( name == "__vm_choose" )
        return BuiltinChoose;
    if ( name == "__vm_mask" )
        return BuiltinMask;
    if ( name == "__vm_jump" )
        return BuiltinJump;
    if ( name == "__vm_fault" )
        return BuiltinFault;
    if ( name == "__vm_cfl_interrupt" )
        return BuiltinCflInterrupt;
    if ( name == "__vm_mem_interrupt" )
        return BuiltinMemInterrupt;
    if ( name == "__vm_interrupt" )
        return BuiltinInterrupt;

    if ( name == "__vm_trace" )
        return BuiltinTrace;

    if ( name == "__vm_make_object" )
        return BuiltinMakeObject;
    if ( name == "__vm_free_object" )
        return BuiltinFreeObject;
    if ( name == "__vm_memcpy" )
        return BuiltinMemcpy;

    if ( name == "__vm_query_varargs" )
        return BuiltinQueryVarargs;
    if ( name == "__vm_query_frame" )
        return BuiltinQueryFrame;
    if ( name == "__vm_query_function" )
        return BuiltinQueryFunction;
    if ( name == "__vm_query_object_size" )
        return BuiltinQueryObjectSize;
    if ( name == "__vm_query_instruction" )
        return BuiltinQueryInstruction;
    if ( name == "__vm_query_variable" )
        return BuiltinQueryVariable;

    if ( f->getIntrinsicID() != llvm::Intrinsic::not_intrinsic )
        return BuiltinIntrinsic; /* not our builtin */

    return NotBuiltin;
}

void Program::builtin( Position p )
{
    Program::Instruction &insn = instruction( p.pc );
    llvm::CallSite CS( p.I );
    llvm::Function *F = CS.getCalledFunction();
    insn.builtin = builtin( F );
    if ( insn.builtin == NotBuiltin )
        throw std::logic_error(
            std::string( "Program::builtin: " ) +
            "Can't call an undefined function: " + F->getName().str() );
}

template< typename Insn >
void Program::insertIndices( Position p )
{
    Insn *I = cast< Insn >( p.I );
    Program::Instruction &insn = instruction( p.pc );

    int shift = insn.values.size();
    insn.values.resize( shift + I->getNumIndices() );

    for ( unsigned i = 0; i < I->getNumIndices(); ++i )
    {
        Slot v( 4, Slot::Constant );
        auto sr = allocateSlot( v );
        _toinit.emplace_back(
            [=]{ initStatic( sr.slot, value::Int< 32 >( I->getIndices()[ i ] ) ); } );
        insn.values[ shift + i ] = sr.slot;
    }
}

Program::Position Program::insert( Position p )
{
    makeFit( function( p.pc ).instructions, p.pc.instruction() );
    Program::Instruction &insn = instruction( p.pc );

    insn.opcode = p.I->getOpcode();
    insn.op = &*p.I;

    if ( dyn_cast< llvm::CallInst >( p.I ) ||
         dyn_cast< llvm::InvokeInst >( p.I ) )
    {
        llvm::CallSite CS( p.I );
        llvm::Function *F = CS.getCalledFunction();
        if ( F ) { // you can actually invoke a label
            switch ( F->getIntrinsicID() ) {
                case llvm::Intrinsic::not_intrinsic:
                    if ( builtin( F ) ) {
                        builtin( p );
                        break;
                    }
                case llvm::Intrinsic::eh_typeid_for:
                case llvm::Intrinsic::trap:
                case llvm::Intrinsic::vastart:
                case llvm::Intrinsic::vacopy:
                case llvm::Intrinsic::vaend:
                case llvm::Intrinsic::stacksave:
                case llvm::Intrinsic::stackrestore:
                case llvm::Intrinsic::umul_with_overflow:
                case llvm::Intrinsic::smul_with_overflow:
                case llvm::Intrinsic::uadd_with_overflow:
                case llvm::Intrinsic::sadd_with_overflow:
                case llvm::Intrinsic::usub_with_overflow:
                case llvm::Intrinsic::ssub_with_overflow:
                    break;
                case llvm::Intrinsic::dbg_declare:
                case llvm::Intrinsic::dbg_value: p.I++; return p;
                default: return lower( p );
            }
        }
    }

    if ( !codepointers )
    {
        insn.values.resize( 1 + p.I->getNumOperands() );
        for ( int i = 0; i < int( p.I->getNumOperands() ); ++i )
            insn.values[ i + 1 ] = insert( p.pc.function(), p.I->getOperand( i ) ).slot;

        if ( isa< llvm::ExtractValueInst >( p.I ) )
            insertIndices< llvm::ExtractValueInst >( p );

        if ( isa< llvm::InsertValueInst >( p.I ) )
            insertIndices< llvm::InsertValueInst >( p );

/* TODO (broken since all constants are now initialised at the end of build())
        if ( auto LPI = dyn_cast< llvm::LandingPadInst >( p.I ) )
        {
            int off = LPI->getNumOperands() - LPI->getNumClauses();
            for ( int i = 0; i < int( LPI->getNumClauses() ); ++i ) {
                if ( LPI->isFilter( i ) )
                    continue;
                auto ptr = constant< ConstPointer >( insn.operand( i + off ) );
                if ( !function( p.pc ).typeID( ptr ) )
                    function( p.pc ).typeIDs.push_back( ptr );
                ASSERT( function( p.pc ).typeID( ptr ) );
            }
        }
*/

        pcmap.insert( std::make_pair( p.I, p.pc ) );
        insn.result() = insert( p.pc.function(), &*p.I ).slot;
    }

    ++ p.I; /* next please */
    p.pc.instruction( p.pc.instruction() + 1 );

    if ( !p.pc.instruction() )
        throw std::logic_error(
            "Program::insert() in " + p.I->getParent()->getParent()->getName().str() +
            "\nToo many instructions in a function, capacity exceeded" );

    return p;
}

void Program::setupRR()
{
    /* null pointers are heap = 0, segment = 0, where segment is an index into
     * the "globals" vector */
    Slot nullpage( 0, Slot::Global );
    _globals.push_back( nullpage );

    for ( auto &f : *module )
        if ( f.getName() == "memset" || f.getName() == "memmove" || f.getName() == "memcpy" )
            f.setLinkage( llvm::GlobalValue::ExternalLinkage );
}

void Program::computeRR()
{
    framealign = 1;

    codepointers = true;
    pass();
    codepointers = false;

    for ( auto var = module->global_begin(); var != module->global_end(); ++ var )
        insert( 0, &*var );

    framealign = 4;
    pass();

    framealign = 1;
    pass();

    coverage.clear();
}

static int getOpcode( llvm::User *u ) {
    if ( auto *i = dyn_cast< llvm::Instruction >( u ) )
        return i->getOpcode();
    if ( auto *c = dyn_cast< llvm::ConstantExpr >( u ) )
        return c->getOpcode();
    return 0;
}

static int getSubOp( llvm::User *u ) {
    if ( auto *armw = dyn_cast< llvm::AtomicRMWInst >( u ) )
        return armw->getOperation();
    if ( auto *cmp = dyn_cast< llvm::CmpInst >( u ) )
        return cmp->getPredicate();
    return 0;
}

void Program::computeStatic()
{
    _ccontext.setup( _globals_size, _constants_size );

    while ( !_toinit.empty() )
    {
        _toinit.front()();
        _toinit.pop_front();
    }

    auto md_func_var = module->getGlobalVariable( "__md_functions" );
    if ( !md_func_var )
        return;

    auto md_func = dyn_cast< llvm::ConstantArray >( md_func_var->getInitializer() );
    ASSERT( valuemap.count( md_func ) );
    auto slotref = valuemap[ md_func ];
    for ( int i = 0; i < int( md_func->getNumOperands() ); ++i )
    {
        auto f = dyn_cast< llvm::ConstantStruct >( md_func->getOperand( i ) );
        const llvm::StructLayout *SL_item = TD.getStructLayout( f->getType() );
        auto name = std::string( f->getOperand( 0 )->getOperand( 0 )->getName(),
                                strlen( "lart.divine.index.name." ), std::string::npos );
        int offset = TD.getTypeAllocSize( md_func->getOperand( 0 )->getType() ) * i +
                     SL_item->getElementOffset( 2 );
        auto pc = functionByName( name );
        if ( !pc.function() )
            continue;
        auto &func = function( pc );
        _ccontext.heap().write( s2hptr( slotref.slot, offset ),
                                value::Int< 32 >( func.datasize + 2 * PointerBytes ) );

        auto instTable = llvm::cast< llvm::GlobalVariable >(
                            f->getOperand( 7 )->getOperand( 0 ) )->getInitializer();
        auto instTableT = cast< llvm::ArrayType >( instTable->getType() );
        ASSERT( valuemap.count( instTable ) );
        auto &instTableSlotref = valuemap[ instTable ];
        // there can be less instructions in DIVINE as it ignores some, such as
        // calls to llvm.dbg.declare
        ASSERT_LEQ( func.instructions.size(), instTableT->getNumElements() );
        auto SL_instMeta = TD.getStructLayout( cast< llvm::StructType >(
                                instTableT->getElementType() ) );
        for ( int j = 0; j < int( func.instructions.size() ); ++j )
        {
            auto &inst = func.instructions[ j ];
            if ( inst.op )
            {
                int base = TD.getTypeAllocSize( instTableT->getElementType() ) * j;
                auto write = [&]( int idx, int val ) {
                    _ccontext.heap().write( s2hptr( instTableSlotref.slot,
                                                    base + SL_instMeta->getElementOffset( idx ) ),
                                            value::Int< 32 >( val ) );
                };
                write( 0, getOpcode( inst.op ) );
                write( 1, getSubOp( inst.op ) );
                write( 2, inst.values.empty() ? 0 : inst.result().offset );
                write( 3, inst.values.empty() ? 0 : inst.result().width );
            }
        }
    }
}

void Program::pass()
{
    CodePointer pc( 1, 0 );
    int _framealign = framealign;

    for ( auto function = module->begin(); function != module->end(); ++ function )
    {
        if ( function->isDeclaration() )
            continue; /* skip */

        auto name = function->getName();

        pc.function( pc.function() + 1 );
        if ( !pc.function() )
            throw std::logic_error(
                "Program::build in " + name.str() +
                "\nToo many functions, capacity exceeded" );

        if ( function->begin() == function->end() )
            throw std::logic_error(
                "Program::build in " + name.str() +
                "\nCan't deal with empty functions" );

        makeFit( functions, pc.function() );
        functionmap[ function ] = pc.function();
        pc.instruction( 0 );

        if ( !codepointers )
        {
            framealign = 1; /* force all args to go in in the first pass */

            auto &pi_function = this->functions[ pc.function() ];
            pi_function.argcount = 0;
            if ( function->hasPersonalityFn() )
                pi_function.personality = insert( 0, function->getPersonalityFn() ).slot;

            for ( auto arg = function->arg_begin(); arg != function->arg_end(); ++ arg ) {
                insert( pc.function(), &*arg );
                ++ pi_function.argcount;
            }

            if ( ( pi_function.vararg = function->isVarArg() ) ) {
                Slot vaptr( PointerBytes, Slot::Local );
                vaptr.type = Slot::Pointer;
                allocateSlot( vaptr, pc.function() );
            }

            framealign = _framealign;

            this->function( pc ).datasize =
                mem::align( this->function( pc ).datasize, framealign );
        }

        for ( auto block = function->begin(); block != function->end(); ++ block )
        {
            blockmap[ &*block ] = pc;
            pc.instruction( pc.instruction() + 1 ); /* leave one out for use as a bb label */

            if ( block->begin() == block->end() )
                throw std::logic_error(
                    std::string( "Program::build: " ) +
                    "Can't deal with an empty BasicBlock in function " + name.str() );

            Program::Position p( pc, block->begin() );
            while ( p.I != block->end() )
                p = insert( p );

            pc = p.pc;
        }
    }

    _globals_size = mem::align( _globals_size, 4 );
}

