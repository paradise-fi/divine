// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2017 Petr Ročkai <code@fixp.eu>
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

#pragma once

#include <brick-bitlevel>
#include <brick-string>
#include <divine/vm/divm.h>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/GlobalAlias.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/CallSite.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
DIVINE_UNRELAX_WARNINGS

#include <divine/vm/lx-code.hpp>
#include <divine/vm/xg-type.hpp>

namespace divine::vm::xg
{


/*
 * The address map keeps track of things that can have their address taken.
 */
struct AddressMap
{
    std::map< llvm::BasicBlock *, CodePointer > _code, _terminator;
    std::map< llvm::GlobalVariable *, GlobalPointer > _global;
    GlobalPointer _next_global = GlobalPointer( 0, 0 );

    GlobalPointer next_global()
    {
        _next_global.object( _next_global.object() + 1 );
        return _next_global;
    }

    bool is_codeptr( llvm::Value *val )
    {
        return ( llvm::isa< llvm::Function >( val ) ||
                 llvm::isa< llvm::BlockAddress >( val ) ||
                 llvm::isa< llvm::BasicBlock >( val ) );
    }

    CodePointer code( llvm::BasicBlock *bb )
    {
        ASSERT( _code.count( bb ) );
        return _code[ bb ];
    }

    CodePointer code( llvm::Function *f )
    {
        return addr( &*f->begin() );
    }

    CodePointer code( llvm::Instruction *i )
    {
        auto bb = code( i->getParent() );
        int seqno = bb.instruction();

        for ( auto &j : *i->getParent() )
        {
            ++ seqno;
            if ( &j == i )
                return CodePointer( bb.function(), seqno );
        }

        UNREACHABLE( "instruction not found" );
    }

    CodePointer code( llvm::Value *v )
    {
        if ( auto F = llvm::dyn_cast< llvm::Function >( v ) )
            return code( F );
        if ( auto B = llvm::dyn_cast< llvm::BasicBlock >( v ) )
            return code( B );
        if ( auto B = llvm::dyn_cast< llvm::BlockAddress >( v ) )
            return code( B->getBasicBlock() );

        UNREACHABLE( "value passed to AddressMap::code() is not a code pointer" );
    }

    CodePointer terminator( llvm::BasicBlock *bb )
    {
        ASSERT( _terminator.count( bb ) );
        return _terminator[ bb ];
    }

    GlobalPointer global( llvm::GlobalVariable *v )
    {
        ASSERT( _global.count( v ) );
        return _global[ v ];
    }

    LocalPointer local( llvm::Instruction *i )
    {
        return LocalPointer( code( i ).instruction(), 0 );
    }

    LocalPointer local( llvm::Argument *a )
    {
        auto f = a->getParent();
        int seqno = 0;
        for ( auto &b : f->args() )
            if ( &b == a )
                return LocalPointer( seqno, 0 );
            else
                ++ seqno;
        UNREACHABLE( "argument not found" );
    }

    /*
     * Obtain an address corresponding to an llvm::Value. The input llvm::Value
     * must be either a code pointer, a global variable or a register / local
     * SSA variable. See also has_addr().
     */
    GenericPointer addr( llvm::Value *v )
    {
        if ( auto GA = llvm::dyn_cast< llvm::GlobalAlias >( v ) )
            return addr( GA->getBaseObject() );

        if ( is_codeptr( v ) )
            return code( v );

        if ( auto I = llvm::dyn_cast< llvm::Instruction >( v ) )
            return local( I );
        if ( auto A = llvm::dyn_cast< llvm::Argument >( v ) )
            return local( A );
        if ( auto G = llvm::dyn_cast< llvm::GlobalVariable >( v ) )
            return global( G );

        v->dump();
        UNREACHABLE( "impossible value in slot_addr()" );
    }

    bool has_addr( llvm::Value *v )
    {
        if ( is_codeptr( v ) )
            return true;
        return has_slot( v );
    }

    bool has_slot( llvm::Value *v )
    {
        return llvm::isa< llvm::Instruction >( v ) || llvm::isa< llvm::Argument >( v ) ||
               llvm::isa< llvm::GlobalVariable >( v );
    }

    void add( llvm::BasicBlock *b, CodePointer p )
    {
        _code[ b ] = p;
    }

    void build( llvm::Module *m )
    {
        ASSERT( _code.empty() );

        for ( auto var = m->global_begin(); var != m->global_end(); ++ var )
            _global[ &*var ] = next_global();

        CodePointer pc( 0, 0 );
        for ( auto &f : *m )
        {
            if ( f.isDeclaration() )
                continue;
            pc.function( pc.function() + 1 );
            pc.instruction( brick::bitlevel::align( f.arg_size() + f.isVarArg(), 4 ) );

            if ( !pc.function() )
                throw std::logic_error( "Capacity exceeded: too many functions." );

            for ( auto &b : f )
            {
                _code[ &b ] = pc;

                for ( auto &i : b )
                    static_cast< void >( i ), pc = pc + 1;

                _terminator[ &b ] = pc;
                pc = pc + 1;
            }
        }
    }
};

namespace
{

int predicate( llvm::Instruction *I )
{
    if ( auto IC = llvm::dyn_cast< llvm::ICmpInst >( I ) )
        return IC->getPredicate();
    if ( auto FC = llvm::dyn_cast< llvm::FCmpInst >( I ) )
        return FC->getPredicate();
    if ( auto ARMW = llvm::dyn_cast< llvm::AtomicRMWInst >( I ) )
        return ARMW->getOperation();

    UNREACHABLE( "bad instruction type in Program::getPredicate" );
}

int predicate( llvm::ConstantExpr *E ) { return E->getPredicate(); }

int intrinsic_id( llvm::Value *v )
{
    auto insn = llvm::dyn_cast< llvm::Instruction >( v );
    if ( !insn || insn->getOpcode() != llvm::Instruction::Call )
        return llvm::Intrinsic::not_intrinsic;
    llvm::CallSite CS( insn );
    auto f = CS.getCalledFunction();
    if ( !f )
        return llvm::Intrinsic::not_intrinsic;
    return f->getIntrinsicID();
}

template< typename IorCE >
int subcode( Types &tg, AddressMap &am, IorCE *I )
{
    if ( I->getOpcode() == llvm::Instruction::GetElementPtr )
        return tg.add( I->getOperand( 0 )->getType() );
    if ( I->getOpcode() == llvm::Instruction::ExtractValue )
        return tg.add( I->getOperand( 0 )->getType() );
    if ( I->getOpcode() == llvm::Instruction::InsertValue )
        return tg.add( I->getType() );
    if ( I->getOpcode() == llvm::Instruction::Alloca )
        return tg.add( I->getType()->getPointerElementType() );
    if ( I->getOpcode() == llvm::Instruction::Call )
        return intrinsic_id( I );
    if ( auto INV = llvm::dyn_cast< llvm::InvokeInst >( I ) ) /* FIXME remove */
        return am.code( INV->getUnwindDest() ).instruction();
    if ( I->getOpcode() == llvm::Instruction::ICmp ||
         I->getOpcode() == llvm::Instruction::FCmp ||
         I->getOpcode() == llvm::Instruction::Invoke ||
         I->getOpcode() == llvm::Instruction::AtomicRMW )
        return predicate( I );
    return 0;
}

lx::Hypercall hypercall( llvm::Function *f )
{
    std::string name = f->getName().str();

    if ( name == "__vm_control" )
        return lx::HypercallControl;
    if ( name == "__vm_choose" )
        return lx::HypercallChoose;
    if ( name == "__vm_fault" )
        return lx::HypercallFault;

    if ( name == "__vm_test_crit" )
        return lx::HypercallTestCrit;
    if ( name == "__vm_test_loop" )
        return lx::HypercallTestLoop;
    if ( brick::string::startsWith( name, "__vm_test_taint" ) )
        return lx::HypercallTestTaint;

    if ( name == "__vm_ctl_set" )
        return lx::HypercallCtlSet;
    if ( name == "__vm_ctl_get" )
        return lx::HypercallCtlGet;
    if ( name == "__vm_ctl_flag" )
        return lx::HypercallCtlFlag;

    if ( name == "__vm_trace" )
        return lx::HypercallTrace;
    if ( name == "__vm_syscall" )
        return lx::HypercallSyscall;

    if ( name == "__vm_obj_make" )
        return lx::HypercallObjMake;
    if ( name == "__vm_obj_clone" )
        return lx::HypercallObjClone;
    if ( name == "__vm_obj_free" )
        return lx::HypercallObjFree;
    if ( name == "__vm_obj_shared" )
        return lx::HypercallObjShared;
    if ( name == "__vm_obj_resize" )
        return lx::HypercallObjResize;
    if ( name == "__vm_obj_size" )
        return lx::HypercallObjSize;

    if ( f->getIntrinsicID() != llvm::Intrinsic::not_intrinsic )
        return lx::NotHypercallButIntrinsic;

    return lx::NotHypercall;
}

_VM_Operand::Type type( llvm::Type *t )
{
    if ( t->isVoidTy() )
        return _VM_Operand::Void;

    if ( t->isPointerTy() )
        return _VM_Operand::Ptr;

    if ( t->isIntegerTy() )
        switch ( t->getPrimitiveSizeInBits() )
        {
            case  1: return _VM_Operand::I1;
            case  8: return _VM_Operand::I8;
            case 16: return _VM_Operand::I16;
            case 32: return _VM_Operand::I32;
            case 64: return _VM_Operand::I64;
            default: return _VM_Operand::Other;
        }

    if ( t->isFloatingPointTy() )
        switch ( t->getPrimitiveSizeInBits() )
        {
            case 32: return _VM_Operand::F32;
            case 64: return _VM_Operand::F64;
            case 80: return _VM_Operand::F80;
            default: return _VM_Operand::Other;
        }

    return _VM_Operand::Agg;
}

}

}
