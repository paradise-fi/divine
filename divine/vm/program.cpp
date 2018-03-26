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
#include <divine/vm/xg-code.hpp>
#include <divine/vm/mem-heap.tpp>
#include <lart/divine/cppeh.h>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Type.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/CallSite.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Module.h>
#include <llvm/ADT/StringMap.h>
DIVINE_UNRELAX_WARNINGS

#include <brick-llvm>

using namespace divine::vm;
using llvm::isa;
using llvm::dyn_cast;
using llvm::cast;

CodePointer Program::functionByName( std::string s )
{
    llvm::Function *f = module->getFunction( s );
    if ( !f )
        return CodePointer();
    if ( xg::hypercall( f ) )
        return CodePointer();
    return _addr.code( f );
}

GenericPointer Program::globalByName( std::string s )
{
    llvm::GlobalVariable *g = module->getGlobalVariable( s );
    if ( g )
        return addr( g );
    else
        return nullPointer();
}

bool Program::isCodePointerConst( llvm::Value *val )
{
    return ( isa< llvm::Function >( val ) ||
             isa< llvm::BlockAddress >( val ) ||
             isa< llvm::BasicBlock >( val ) );
}

bool Program::isCodePointer( llvm::Value *val )
{
    if ( isa< llvm::BlockAddress >( val ) )
        return true;

    if ( auto ty = dyn_cast< llvm::PointerType >( val->getType() ) )
        return ty->getElementType()->isFunctionTy();

    return isCodePointerConst( val );
}

CodePointer Program::getCodePointer( llvm::Value *val )
{
    if ( auto B = dyn_cast< llvm::BasicBlock >( val ) ) {
        return _addr.code( B );
    } else if ( auto B = dyn_cast< llvm::BlockAddress >( val ) ) {
        return _addr.code( B->getBasicBlock() );
    } else if ( auto F = dyn_cast< llvm::Function >( val ) ) {
        if ( F->isDeclaration() && xg::hypercall( F ) == lx::NotHypercall )
            throw brick::except::Error(
                "Program::insert: " +
                std::string( "Unresolved symbol (function): " ) + F->getName().str() );

        if ( xg::hypercall( F ) )
            return CodePointer();

        return _addr.code( F );
    }
    return CodePointer();
}

Program::Slot Program::initSlot( llvm::Value *val, Slot::Location loc )
{
    auto t = val->getType();
    Slot result( loc, xg::type( t ),
                 t->isSized() ? 8 * TD.getTypeAllocSize( t ) : 0 );

    if ( isCodePointer( val ) )
        result.type = Slot::PtrC;

    if ( auto CDS = dyn_cast< llvm::ConstantDataSequential >( val ) )
        ASSERT_EQ( result.width(), 8 * CDS->getNumElements() * CDS->getElementByteSize() );

    if ( isa< llvm::AllocaInst >( val ) || xg::intrinsic_id( val ) == llvm::Intrinsic::stacksave )
    {
        ASSERT_EQ( loc, Slot::Local );
        result.type = Slot::PtrA;
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
    for ( int i = 0; i < int( list->getNumOperands() ); ++i ) {
        auto item = dyn_cast< llvm::MDNode >( list->getOperand( i ) );
        if ( item == id )
            return true;
    }
    return false;
}

void Program::overlaySlot( int fun, Slot &result, llvm::Value *val )
{
    if ( !result.width() )
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
    if ( !insn )
        goto alloc;

    if ( insn->getOpcode() == llvm::Instruction::BitCast )
    {
        auto src = insert( fun, insn->getOperand( 0 ) );
        ASSERT_EQ( src.width(), result.width() );
        result.offset = src.offset;
        result.location = src.location;
        return;
    }

    if ( !insn->getMetadata( "lart.interference" ) )
        goto alloc;

    ASSERT( !f.instructions.empty() );
    c.resize( f.framesize );

    for ( result.offset = 2 * PointerBytes; result.offset < f.framesize; ) {
        iter ++;
        if ( result.offset + result.size() > f.framesize )
            goto alloc;

        bool good = true;
        for ( int i = 0; good && i < result.size(); ++i )
            for ( auto v : c[ result.offset + i ] )
                if ( lifetimeOverlap( val, v ) ) {
                    ASSERT( valuemap.find( v ) != valuemap.end());
                    auto s = valuemap[ v ];
                    result.offset = s.offset + s.size();
                    good = false;
                    break;
                }

        if ( good )
            goto out;
    }
alloc:
    result.offset = f.framesize;
    f.framesize += result.size();
    c.resize( f.framesize );
out:
    c[ result.offset ].push_back( val );
}

Program::Slot Program::insert( int function, llvm::Value *val, bool )
{
    Slot::Location sl;

    /* global scope is only applied to GlobalVariable initializers */
    if ( !function || isa< llvm::Constant >( val ) || isCodePointerConst( val ) )
        sl = Slot::Const;
    else
        sl = Slot::Local;

    auto val_i = valuemap.find( val );
    if ( val_i != valuemap.end() )
    {
        if ( !isa< llvm::BitCastInst >( val ) )
            ASSERT_EQ( val_i->second.location, sl );
        return val_i->second;
    }

    if ( auto GA = dyn_cast< llvm::GlobalAlias >( val ) )
    {
        auto r = insert( function, const_cast< llvm::GlobalObject * >( GA->getBaseObject() ) );
        valuemap.insert( std::make_pair( val, r ) );
        ASSERT( r.location != Slot::Invalid );
        return r;
    }

    if ( auto CE = dyn_cast< llvm::ConstantExpr >( val ) )
        initSubcode( CE );

    makeFit( functions, function );

    auto slot_i = initSlot( val, sl );

    if ( slot_i.size() % framealign )
        return Slot(); /* ignore for now, later pass will assign this one */

    Slot slot;

    if ( auto G = dyn_cast< llvm::GlobalVariable >( val ) )
    {
        /* G is a pointer type, G's initializer is of the actual value type  */
        if ( !G->hasInitializer() )
            throw std::logic_error(
                "Program::insert: " +
                std::string( "Unresolved symbol (global variable): " ) +
                G->getValueName()->getKey().str() );

        slot = allocateSlot( slot_i, function, nullptr );
        insert( 0, G->getInitializer() );
        Slot slot_val = initSlot( G->getInitializer(), G->isConstant() ? Slot::Const : Slot::Global );

        if ( G->isConstant() )
        {
            int idx = _addr.addr( G ).object();
            makeFit( _globals, idx );
            ASSERT( valuemap.count( G->getInitializer() ) );
            _globals[ idx ] = valuemap[ G->getInitializer() ];
        }
        else
            globalmap[ G ] = allocateSlot( slot_val, 0, G );
    }
    else slot = allocateSlot( slot_i, function, val );

    if ( isa< llvm::Constant >( val ) || isCodePointerConst( val ) )
        _toinit.emplace_back( [=]{ initConstant( slot, val ); } );

    ASSERT( slot.location != Slot::Invalid );
    valuemap.insert( std::make_pair( val, slot ) );

    if ( auto U = dyn_cast< llvm::User >( val ) )
        for ( int i = 0; i < int( U->getNumOperands() ); ++i )
            if ( !isa< llvm::MetadataAsValue >( U->getOperand( i ) ) )
                insert( function, U->getOperand( i ) );

    return slot;
}

void Program::hypercall( Position p )
{
    Program::Instruction &insn = instruction( p.pc );
    llvm::CallSite CS( p.I );
    llvm::Function *F = CS.getCalledFunction();
    if ( insn.opcode != llvm::Instruction::Call )
        throw std::logic_error(
            std::string( "Program::hypercall: " ) +
            "Cannot 'invoke' a hypercall, use 'call' instead: " + F->getName().str() );
    insn.opcode = lx::OpHypercall;
    insn.subcode = xg::hypercall( F );
    if ( insn.subcode == lx::NotHypercall )
        throw std::logic_error(
            std::string( "Program::hypercall: " ) +
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
        Slot v( Slot::Const, Slot::I32 );
        auto slot = allocateSlot( v );
        _toinit.emplace_back(
            [=]{ initConstant( slot, value::Int< 32 >( I->getIndices()[ i ] ) ); } );
        insn.values[ shift + i ] = slot;
    }
}

Program::Position Program::insert( Position p )
{
    makeFit( function( p.pc ).instructions, p.pc.instruction() );
    Program::Instruction &insn = instruction( p.pc );

    insn.opcode = p.I->getOpcode();
    insn.subcode = initSubcode( &*p.I );

    if ( insn.opcode == llvm::Instruction::BitCast )
    {
        insn.opcode = lx::OpDbg;
        insn.subcode = lx::DbgBitCast;
    }

    if ( dyn_cast< llvm::CallInst >( p.I ) ||
         dyn_cast< llvm::InvokeInst >( p.I ) )
    {
        llvm::CallSite CS( p.I );
        llvm::Function *F = CS.getCalledFunction();
        if ( F ) // you can actually invoke a label
            switch ( F->getIntrinsicID() )
            {
                case llvm::Intrinsic::not_intrinsic:
                    if ( xg::hypercall( F ) )
                        hypercall( p );
                    break;
                case llvm::Intrinsic::dbg_declare:
                    insn.subcode = lx::DbgDeclare;
                    insn.opcode = lx::OpDbg;
                    break;
                case llvm::Intrinsic::dbg_value:
                    insn.subcode = lx::DbgValue;
                    insn.opcode = lx::OpDbg;
                    break;
                default: ;
            }
        if ( F && is_debug.count( F ) )
        {
            if ( dyn_cast< llvm::InvokeInst >( p.I ) )
                throw std::logic_error( "Cannot turn an 'invoke' into a 'dbg.call'." );
            insn.opcode = lx::OpDbgCall;
        }
    }

    if ( !codepointers )
    {
        int operands = p.I->getNumOperands() - ( insn.opcode == lx::OpHypercall );
        insn.values.resize( 1 + operands );
        for ( int i = 0; i < operands; ++i )
            if ( !isa< llvm::MetadataAsValue >( p.I->getOperand( i ) ) )
                insn.values[ i + 1 ] = insert( p.pc.function(), p.I->getOperand( i ) );
        insn.values[0] = insert( p.pc.function(), &*p.I );

        if ( auto PHI = dyn_cast< llvm::PHINode >( p.I ) )
        {
            auto nPHI = dyn_cast< llvm::PHINode >( std::next( p.I ) );
            for ( unsigned idx = 0; idx < PHI->getNumOperands(); ++idx )
            {
                if ( nPHI ) ASSERT_EQ( PHI->getIncomingBlock( idx ), nPHI->getIncomingBlock( idx ) );
                auto from = _addr.terminator( PHI->getIncomingBlock( idx ) );
                auto slot = allocateSlot( Slot( Slot::Const, Slot::PtrC ) );
                _toinit.emplace_back( [=]{ initConstant( slot, value::Pointer( from ) ); } );
                insn.values.push_back( slot );
            }
        }

        if ( isa< llvm::ExtractValueInst >( p.I ) )
            insertIndices< llvm::ExtractValueInst >( p );

        if ( isa< llvm::InsertValueInst >( p.I ) )
            insertIndices< llvm::InsertValueInst >( p );
    }

    auto block = p.I->getParent();
    ++ p.I; /* next please */
    p.pc = p.pc + 1;
    if ( p.I != block->end() )
        ASSERT_EQ( _addr.code( &*p.I ), p.pc );

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
    Slot nullpage( Slot::Global );
    _globals.push_back( nullpage );

    for ( auto &f : *module )
        if ( f.getName() == "memset" || f.getName() == "memmove" || f.getName() == "memcpy" )
            f.setLinkage( llvm::GlobalValue::ExternalLinkage );
}

void Program::computeRR()
{
    _addr.build( module );
    brick::llvm::enumerateFunctionsForAnno( "divine.debugfn", *module, [this]( llvm::Function *f )
                                            {
                                                is_debug.insert( f );
                                            } );
    brick::llvm::enumerateFunctionsForAnno( "divine.trapfn", *module, [this]( llvm::Function *f )
                                            {
                                                is_trap.insert( _addr.code( f ).function() );
                                            } );

    framealign = 1;

    codepointers = true;
    pass();
    codepointers = false;

    for ( auto var = module->global_begin(); var != module->global_end(); ++ var )
        insert( 0, &*var );

    framealign = 4;
    pass();

    framealign = 2;
    pass();

    framealign = 1;
    pass();

    _types.reset( new LXTypes( _ccontext._heap, _types_gen.emit( _ccontext._heap ) ) );
    coverage.clear();
}

void Program::computeStatic()
{
    _ccontext.setup( _globals_size, _constants_size );

    while ( !_toinit.empty() )
    {
        _toinit.front()();
        _toinit.pop_front();
    }

    for ( auto var = module->global_begin(); var != module->global_end(); ++ var )
    {
        if ( !var->getInitializer() || var->isConstant() )
            continue;
        ASSERT( globalmap.find( var ) != globalmap.end() );
        ASSERT( valuemap.find( var->getInitializer() ) != valuemap.end() );
        auto location = valuemap[ var->getInitializer() ];
        auto target = globalmap[ var ];
        _ccontext.heap().copy( s2hptr( location ), s2hptr( target ), location.size() );
    }

    auto md_func_var = module->getGlobalVariable( "__md_functions" );
    if ( !md_func_var )
        return;

    auto md_func = dyn_cast< llvm::ConstantArray >( md_func_var->getInitializer() );
    ASSERT( valuemap.count( md_func ) );
    auto slot = valuemap[ md_func ];

    auto md2name = [&]( auto op )
    {
        return std::string( dyn_cast< llvm::ConstantStruct >( op )->
                                getOperand( 0 )->getOperand( 0 )->getName(),
                            strlen( "lart.divine.index.name." ), std::string::npos );
    };

    auto md2pc = [&]( auto op ) { return functionByName( md2name( op ) ); };

    auto instTableT = llvm::cast< llvm::PointerType >(
                          md_func->getOperand( 0 )->getOperand( 7 )->getType() )->getElementType();
    int instTotal = 0;

    for ( int i = 0; i < int( md_func->getNumOperands() ); ++i )
    {
        auto pc = md2pc( md_func->getOperand( i ) );
        if ( !pc.function() )
            continue;

        instTotal += function( pc ).instructions.size();
    }

    auto instTableObj = _ccontext.heap().make( instTotal * TD.getTypeAllocSize( instTableT ) );
    metadata_ptr.insert( instTableObj.cooked() );
    int instOffset = 0;

    for ( int i = 0; i < int( md_func->getNumOperands() ); ++i )
    {
        auto op = dyn_cast< llvm::ConstantStruct >( md_func->getOperand( i ) );
        const llvm::StructLayout *SL_item = TD.getStructLayout( op->getType() );
        auto pc = md2pc( op );
        if ( !pc.function() )
            continue;
        auto &func = function( pc );

        auto writeMetaElem = [&]( int idx, auto val ) {
            int offset = TD.getTypeAllocSize( md_func->getOperand( 0 )->getType() ) * i
                          + SL_item->getElementOffset( idx );
            _ccontext.heap().write( s2hptr( slot, offset ), val );
        };
        writeMetaElem( 2, value::Int< 32 >( func.framesize ) ); // frame size
        writeMetaElem( 6, value::Int< 32 >( func.instructions.size() ) ); // inst count

        // create and write write instruction table
        auto instTable = instTableObj + instOffset;
        int instTableSize = TD.getTypeAllocSize( instTableT ) * func.instructions.size();
        writeMetaElem( 7, instTable );

        auto writeInst = [&]( int val ) {
            _ccontext.heap().write_shift( instTable, value::Int< 32 >( val ) );
        };
        for ( int j = 0; j < int( func.instructions.size() ); ++j )
        {
            auto &inst = func.instructions[ j ];
            writeInst( inst.opcode );
            writeInst( inst.subcode );
            writeInst( inst.values.empty() ? 0 : inst.result().offset );
            writeInst( inst.values.empty() ? 0 : inst.result().size() ); /* bytes? */
        }
        ASSERT_EQ( instTable.cooked().offset() - instOffset, instTableSize );

        instOffset += instTableSize;
    }
}

void Program::pass()
{
    for ( auto &function : *module )
    {
        if ( function.isDeclaration() )
            continue; /* skip */

        auto pc = _addr.code( &function );
        makeFit( this->functions, pc.function() );
        makeFit( this->function( pc ).instructions, pc.instruction() );

        if ( !codepointers )
        {
            CodePointer apc( pc.function(), 0 );

            auto &pi_function = this->functions[ pc.function() ];
            pi_function.argcount = 0;
            if ( function.hasPersonalityFn() )
                pi_function.personality = insert( 0, function.getPersonalityFn() );

            for ( auto &arg : function.args() )
            {
                this->instruction( apc ).opcode = lx::OpArg;
                auto &vs = this->instruction( apc ).values;
                makeFit( vs, 1 );
                vs[ 0 ] = insert( pc.function(), &arg );
                apc = apc + 1;
                ++ pi_function.argcount;
            }

            if ( ( pi_function.vararg = function.isVarArg() ) )
            {
                if ( framealign == 4 )
                {
                    Slot vaptr( Slot::Local );
                    vaptr.type = Slot::Ptr;
                    overlaySlot( pc.function(), vaptr, nullptr );
                    auto &vs = this->instruction( apc ).values;
                    makeFit( vs, 1 );
                    vs[ 0 ] = vaptr;
                    this->instruction( apc ).opcode = lx::OpArg;
                }
                apc = apc + 1;
            }

            while ( apc.instruction() % 4 )
                apc = apc + 1;

            ASSERT_EQ( apc, pc );
        }

        for ( auto &block : function )
        {
            Program::Position p( _addr.code( &block ), block.begin() );
            makeFit( this->function( p.pc ).instructions, p.pc.instruction() );
            this->instruction( p.pc ).opcode = lx::OpBB;
            p.pc = p.pc + 1;
            while ( p.I != block.end() )
                p = insert( p );
        }
    }

    _globals_size = brick::mem::align( _globals_size, 4 );
}

lx::Slot Program::allocateSlot( Slot slot, int function, llvm::Value *val )
{
    switch ( slot.location )
    {
        case Slot::Const:
            slot.offset = _constants_size;
            _constants_size = brick::mem::align( _constants_size + slot.size(), 4 );
            if ( val && _addr.has_slot( val ) )
            {
                int idx = _addr.addr( val ).object();
                makeFit( _globals, idx );
                _globals[ idx ] = slot;
            }
            return slot;
        case Slot::Global:
            slot.offset = _globals_size;
            _globals_size = brick::mem::align( _globals_size + slot.size(), 4 );
            ASSERT( val && _addr.has_slot( val ) );
            {
                int idx = _addr.addr( val ).object();
                makeFit( _globals, idx );
                _globals[ idx ] = slot;
            }
            return slot;
        case Slot::Local:
        {
            ASSERT( function );
            ASSERT( val );
            overlaySlot( function, slot, val );
            return slot;
        }
        default:
            UNREACHABLE( "invalid slot location" );
    }
}

template< typename H >
std::pair< HeapPointer, HeapPointer > Program::exportHeap( H &target )
{
    auto cp = value::Pointer(
            mem::heap::clone( _ccontext._heap, target, _ccontext.constants() ) );

    if ( !_globals_size )
        return std::make_pair( cp.cooked(), nullPointer() );

    auto gp = target.make( _globals_size );
    target.copy( _ccontext._heap, _ccontext.globals(),
                    gp.cooked(), _globals_size );
    target.shared( gp.cooked(), true );
    return std::make_pair( cp.cooked(), gp.cooked() );
}

template std::pair< HeapPointer, HeapPointer > Program::exportHeap< mem::CowHeap >( mem::CowHeap & );
template std::pair< HeapPointer, HeapPointer > Program::exportHeap< mem::MutableHeap >( mem::MutableHeap & );
template std::pair< HeapPointer, HeapPointer > Program::exportHeap< mem::SmallHeap >( mem::SmallHeap & );
